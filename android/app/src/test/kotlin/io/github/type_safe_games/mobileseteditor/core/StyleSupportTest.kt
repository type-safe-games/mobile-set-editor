// Mobile Set Editor - Unsupported styles from the status file (#72)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.core

import org.junit.Assert.assertEquals
import org.junit.Test

class StyleSupportTest {
    private val status = StyleSupport.parse(
        """
        # comment
        magic-m15.mse-style	2024-05-26	supported
        magic-doomtown.mse-style	2020-01-01	unsupported
        magic-m15-loyalty.mse-style	2024-06-07	template-bug
        magic-future.mse-style	1	some-status-from-a-newer-tool
        """.trimIndent(),
    )

    @Test
    fun statusByNameAndVersion() {
        assertEquals(Support.Supported, status.of("magic-m15.mse-style", "2024-05-26"))
        assertEquals(Support.Unsupported, status.of("magic-doomtown.mse-style", "2020-01-01"))
        // fails only where the reference desktop editor fails too (#38)
        assertEquals(Support.TemplateBug, status.of("magic-m15-loyalty.mse-style", "2024-06-07"))
        // a status this version doesn't know is the cautious one
        assertEquals(Support.Unsupported, status.of("magic-future.mse-style", "1"))
        // another version than the one tested, or not tested at all
        assertEquals(Support.Untested, status.of("magic-m15.mse-style", "2025-01-01"))
        assertEquals(Support.Untested, status.of("someone-elses.mse-style", "1"))
    }
}
