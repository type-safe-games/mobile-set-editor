// Mobile Set Editor - rendering Cards through a priority queue, with a cache (ADR 0009)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.render

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import io.github.type_safe_games.mobileseteditor.core.FieldRegion

/** A Card at a width in pixels: what a render is cached by */
data class RenderKey(val uid: String, val width: Int)

/** A finished render; `stale` while a newer one is on its way (after an edit) */
class Rendered<I>(val key: RenderKey, val image: I, val regions: List<FieldRegion>, val stale: Boolean = false)

/** Renders Cards for the service; implemented over the core (CoreCardRenderer), faked in tests */
interface CardRenderer<I> {
    /**
     * Render a Card at a width, on the core thread. [spare] gives an image of a size to reuse, if
     * the service has one (a render of another Card that left the cache). Null if the Card
     * can't be rendered.
     */
    suspend fun render(key: RenderKey, spare: (width: Int, height: Int) -> I?): Pair<I, List<FieldRegion>>?

    /** Bytes an image takes */
    fun byteSize(image: I): Long

    fun width(image: I): Int
    fun height(image: I): Int
}

/**
 * The render queue and cache of one open Set (ADR 0009).
 *
 * Screens say which Cards they show ([want]): the visible ones, and the ones worth rendering
 * ahead (neighbours, thumbnails). One worker renders the wanted Cards one at a time, visible ones
 * first; pre-renders that are no longer wanted are dropped from the queue. Run the service's
 * scope off the core thread: each render is then its own dispatch onto the core thread, so an
 * edit the host runs meanwhile (also on the core thread) goes ahead of the next pre-render.
 *
 * Finished renders are kept in an LRU cache with a byte budget, shared by full renders and
 * thumbnails; the Cards wanted as visible are never evicted. Images that leave the cache are kept
 * (a few, within the budget) for reuse by renders of the same size. After an edit, [invalidate]
 * marks the stale Cards' renders stale and renders the wanted ones again, visible ones first.
 *
 * Screens observe renders as state ([observe]) instead of calling the core.
 */
class RenderService<I>(
    private val renderer: CardRenderer<I>,
    val budgetBytes: Long,
    scope: CoroutineScope,
    private val maxSpares: Int = 2,
) {
    private val lock = Any()
    private val flows = HashMap<RenderKey, MutableStateFlow<Rendered<I>?>>()
    /** the cache, least recently used first */
    private val cache = LinkedHashMap<RenderKey, Rendered<I>>(16, 0.75f, true)
    private val spares = ArrayDeque<I>()
    private var visible: List<RenderKey> = emptyList()
    private var preRender: List<RenderKey> = emptyList()
    /** keys whose render is stale or missing and wanted */
    private val wake = Channel<Unit>(Channel.CONFLATED)

    /** what the worker is rendering now, if anything */
    @Volatile var rendering: RenderKey? = null
        private set
    /** keys rendered so far, in order (for tests and diagnostics) */
    val renderLog: MutableList<RenderKey> = java.util.Collections.synchronizedList(mutableListOf())

    init {
        scope.launch {
            while (true) {
                val key = synchronized(lock) { next() }
                if (key == null) {
                    wake.receive()
                    continue
                }
                rendering = key
                val result = try {
                    renderer.render(key) { w, h -> synchronized(lock) { takeSpare(w, h) } }
                } finally {
                    rendering = null
                }
                renderLog += key
                synchronized(lock) { store(key, result) }
            }
        }
    }

    /** The render of a Card at a width, as state: null until there is one */
    fun observe(key: RenderKey): StateFlow<Rendered<I>?> = synchronized(lock) { flowOf(key).asStateFlow() }

    /**
     * Which Cards are shown ([visible], rendered first) and which to render ahead ([preRender], in
     * this order). Replaces what was wanted before: pre-renders not asked for again are dropped.
     */
    fun want(visible: List<RenderKey>, preRender: List<RenderKey> = emptyList()) {
        synchronized(lock) {
            this.visible = visible
            this.preRender = preRender.filter { it !in visible }
            // the visible ones are the most recently used
            for (k in visible) cache[k]
        }
        wake.trySend(Unit)
    }

    /**
     * The Set changed (an ActionResult's Stale): [all] Cards, or the Cards [uids]. Their renders
     * become stale; the wanted ones are rendered again, the others dropped from the cache.
     */
    fun invalidate(all: Boolean, uids: Collection<String> = emptyList()) {
        synchronized(lock) {
            val wanted = (visible + preRender).toSet()
            val affected = cache.keys.filter { all || it.uid in uids }
            for (k in affected) {
                val r = cache.getValue(k)
                if (k in wanted) {
                    val staleRender = Rendered(k, r.image, r.regions, stale = true)
                    cache[k] = staleRender
                    flows[k]?.value = staleRender
                } else {
                    cache.remove(k)
                    flows[k]?.value = null
                    addSpare(r.image)
                }
            }
        }
        wake.trySend(Unit)
    }

    /** Bytes held now: the cache and the spare images */
    val cachedBytes: Long get() = synchronized(lock) { cache.values.sumOf { renderer.byteSize(it.image) } + spares.sumOf { renderer.byteSize(it) } }

    val cachedKeys: Set<RenderKey> get() = synchronized(lock) { cache.keys.toSet() }

    // ------------------------------------------------------------------- (under lock)

    private fun flowOf(key: RenderKey) = flows.getOrPut(key) { MutableStateFlow(cache[key]) }

    private fun needsRender(key: RenderKey): Boolean {
        val r = cache[key]
        return r == null || r.stale
    }

    /** The next key to render: a visible one first, then the pre-renders in order */
    private fun next(): RenderKey? =
        visible.firstOrNull { needsRender(it) } ?: preRender.firstOrNull { needsRender(it) }

    private fun store(key: RenderKey, result: Pair<I, List<FieldRegion>>?) {
        if (result == null) return
        val (image, regions) = result
        // (the render it replaces may still be on screen, so it is not reused)
        cache.remove(key)
        val rendered = Rendered(key, image, regions)
        cache[key] = rendered
        flows[key]?.value = rendered
        evict()
    }

    /** Drop the least recently used renders (never a visible one) until within the budget */
    private fun evict() {
        var bytes = cache.values.sumOf { renderer.byteSize(it.image) } + spares.sumOf { renderer.byteSize(it) }
        while (bytes > budgetBytes && spares.isNotEmpty()) bytes -= renderer.byteSize(spares.removeFirst())
        if (bytes <= budgetBytes) return
        val it = cache.entries.iterator()
        while (bytes > budgetBytes && it.hasNext()) {
            val e = it.next()
            if (e.key in visible) continue
            it.remove()
            flows[e.key]?.value = null
            bytes -= renderer.byteSize(e.value.image)
        }
    }

    private fun addSpare(image: I) {
        spares.addLast(image)
        while (spares.size > maxSpares) spares.removeFirst()
    }

    private fun takeSpare(width: Int, height: Int): I? {
        val i = spares.indexOfFirst { renderer.width(it) == width && renderer.height(it) == height }
        return if (i >= 0) spares.removeAt(i) else null
    }
}
