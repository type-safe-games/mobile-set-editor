// Mobile Set Editor - the render queue and cache, with a fake renderer (#59)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor.render

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.runCurrent
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mobileseteditor.core.FieldRegion

class FakeImage(val width: Int, val height: Int, var content: String)

/** Renders a key into a 10x10 image (100 "bytes"); renders of keys in [gates] wait for their gate */
class FakeRenderer : CardRenderer<FakeImage> {
    val gates = HashMap<RenderKey, CompletableDeferred<Unit>>()
    val reused = mutableListOf<RenderKey>()
    var renders = 0

    override suspend fun render(key: RenderKey, spare: (Int, Int) -> FakeImage?): Pair<FakeImage, List<FieldRegion>> {
        gates[key]?.await()
        renders++
        val image = spare(10, 10)?.also { reused += key } ?: FakeImage(10, 10, "")
        image.content = "${key.uid}#$renders"
        return image to listOf(FieldRegion("name", 0.0, 0.0, 10.0, 2.0, 0.0, 0, true))
    }

    override fun byteSize(image: FakeImage) = 100L
    override fun width(image: FakeImage) = image.width
    override fun height(image: FakeImage) = image.height
}

@OptIn(ExperimentalCoroutinesApi::class)
class RenderServiceTest {
    private fun key(uid: String) = RenderKey(uid, 10)

    /** Let the worker (in backgroundScope, which advanceUntilIdle leaves alone) run to idle */
    private fun TestScope.settle() = repeat(20) { runCurrent() }

    private fun TestScope.service(renderer: FakeRenderer, budget: Long = 10_000) =
        RenderService(renderer, budget, backgroundScope)

    @Test
    fun visibleFirstThenPreRendersInOrder() = runTest {
        val r = FakeRenderer()
        val s = service(r)
        s.want(listOf(key("a")), listOf(key("b"), key("c")))
        settle()
        assertEquals(listOf(key("a"), key("b"), key("c")), s.renderLog.toList())
        assertEquals("a#1", s.observe(key("a")).value!!.image.content)
    }

    @Test
    fun aNewVisibleCardGoesAheadOfWaitingPreRenders() = runTest {
        val r = FakeRenderer()
        val gate = CompletableDeferred<Unit>()
        r.gates[key("b")] = gate
        val s = service(r)
        s.want(listOf(key("a")), listOf(key("b"), key("c")))
        settle()
        assertEquals(key("b"), s.rendering) // a done, b in flight
        s.want(listOf(key("d")), listOf(key("b"), key("c")))
        gate.complete(Unit)
        settle()
        assertEquals(listOf(key("a"), key("b"), key("d"), key("c")), s.renderLog.toList())
    }

    @Test
    fun preRendersNoLongerWantedAreDropped() = runTest {
        val r = FakeRenderer()
        val gate = CompletableDeferred<Unit>()
        r.gates[key("a")] = gate
        val s = service(r)
        s.want(listOf(key("a")), listOf(key("b"), key("c"), key("d")))
        settle()
        s.want(listOf(key("x")), listOf(key("y")))
        gate.complete(Unit)
        settle()
        assertEquals(listOf(key("a"), key("x"), key("y")), s.renderLog.toList())
    }

    @Test
    fun invalidationRendersWantedCardsAgainVisibleFirst() = runTest {
        val r = FakeRenderer()
        val s = service(r)
        s.want(listOf(key("a")), listOf(key("b")))
        settle()
        s.want(listOf(key("b")), listOf(key("a"))) // b is shown now
        settle()
        s.renderLog.clear()
        s.invalidate(all = true)
        // stale renders stay visible until the new ones are there
        assertTrue(s.observe(key("a")).value!!.stale)
        settle()
        assertEquals(listOf(key("b"), key("a")), s.renderLog.toList())
        assertTrue(!s.observe(key("b")).value!!.stale)

        // only the stale uids, and an unwanted stale render leaves the cache
        s.want(listOf(key("b")))
        s.renderLog.clear()
        s.invalidate(all = false, uids = listOf("a"))
        settle()
        assertTrue(s.renderLog.isEmpty())
        assertNull(s.observe(key("a")).value)
        assertTrue(key("a") !in s.cachedKeys)
    }

    @Test
    fun theCacheStaysWithinItsBudgetAndKeepsTheVisibleCard() = runTest {
        val r = FakeRenderer()
        val s = service(r, budget = 350) // room for 3 renders of 100
        for (i in 0 until 40) {
            val visible = key("c$i")
            s.want(listOf(visible), listOf(key("c${i + 1}"), key("c${i + 2}")))
            settle()
            assertTrue("over budget: ${s.cachedBytes}", s.cachedBytes <= 350)
            assertNotNull(s.observe(visible).value)
            assertTrue(visible in s.cachedKeys)
        }
        // stepping forward found the next Card already rendered: one render per step after the first ones
        assertEquals(42, r.renders)
    }

    @Test
    fun imagesThatLeaveTheCacheAreReusedForTheSameSize() = runTest {
        val r = FakeRenderer()
        val s = service(r)
        s.want(listOf(key("a")), listOf(key("b")))
        settle()
        val bImage = s.observe(key("b")).value!!.image
        s.want(listOf(key("a")))
        s.invalidate(all = false, uids = listOf("b")) // b is no longer wanted: its image is spare
        s.want(listOf(key("a")), listOf(key("c")))
        settle()
        assertEquals(listOf(key("c")), r.reused)
        assertSame(bImage, s.observe(key("c")).value!!.image)
    }
}
