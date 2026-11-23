// Mobile Set Editor - which Styles this version renders like the desktop: Unsupported styles (ADR 0010, #72)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.core

import android.content.Context

/** How well this version renders a Style (CONTEXT.md "Unsupported style") */
enum class Support {
    /** it passed the Android golden check */
    Supported,
    /** it failed it: it may not look as on the desktop (it can still be used) */
    Unsupported,
    /** not in the golden corpus, or another version than the one tested */
    Untested,
}

/**
 * The Style support status this version was tested with: the asset style-status.tsv, written by
 * an Android golden run (tools/goldens/goldens.py --android --status-out). Lines are
 * "package \t version \t supported|unsupported"; '#' starts a comment.
 */
class StyleSupport(private val tested: Map<String, Pair<String, Boolean>>) {
    fun of(style: String, version: String): Support {
        val (testedVersion, supported) = tested[style] ?: return Support.Untested
        if (testedVersion.isNotEmpty() && version.isNotEmpty() && testedVersion != version) return Support.Untested
        return if (supported) Support.Supported else Support.Unsupported
    }

    fun of(t: TemplateInfo) = of(t.name, t.version)

    companion object {
        const val ASSET = "style-status.tsv"

        fun parse(text: String) = StyleSupport(
            text.lineSequence().map { it.trim() }.filter { it.isNotEmpty() && !it.startsWith("#") }.mapNotNull { line ->
                val f = line.split('\t')
                if (f.size < 3) null else f[0] to (f[1] to (f[2] == "supported"))
            }.toMap(),
        )

        @Volatile private var loaded: StyleSupport? = null

        /** This version's status, from the app's assets (read once) */
        fun get(context: Context): StyleSupport = loaded ?: synchronized(this) {
            loaded ?: runCatching { context.assets.open(ASSET).use { parse(it.readBytes().toString(Charsets.UTF_8)) } }
                .getOrElse { StyleSupport(emptyMap()) }
                .also { loaded = it }
        }
    }
}
