// Mobile Set Editor - a Card's Style where Cards are made: adding one in a Style, and a picker (#87)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import io.github.type_safe_games.mobileseteditor.core.Core
import io.github.type_safe_games.mobileseteditor.core.TemplateInfo

/** The installed Styles of the open Set's Game, for pickers and labels (empty until read) */
@Composable
fun rememberGameStyles(model: SetViewModel): List<TemplateInfo> {
    val state by model.state.collectAsState()
    var styles by remember { mutableStateOf<List<TemplateInfo>>(emptyList()) }
    LaunchedEffect(state.session) {
        if (state.session == 0L) return@LaunchedEffect
        val game = model.game()
        styles = Core.listTemplates().templates.filter { it.type == "style" && it.game == game }
    }
    return styles
}

/** A Style's short name for labels: its picker name, or the package name without its extension */
fun styleLabel(styles: List<TemplateInfo>, stylesheet: String): String =
    styles.firstOrNull { it.name == stylesheet }?.shortName ?: stylesheet.removeSuffix(".mse-style")

/**
 * The menu under an "Add Card" button (#87): a Card in the Set's Style (the default, first), in each
 * other Style the Set's Cards already use, or in any other installed Style. [onAdd] gets the Style's
 * package name, or "" for the Set's.
 */
@Composable
fun AddCardMenu(model: SetViewModel, styles: List<TemplateInfo>, expanded: Boolean, onDismiss: () -> Unit, onAdd: (String) -> Unit) {
    val state by model.state.collectAsState()
    var picking by remember { mutableStateOf(false) }
    var setStyle by remember { mutableStateOf("") }
    LaunchedEffect(state.session, state.cardStyles) {
        if (state.session != 0L) setStyle = Core.sessionInfo(state.session).stylesheet
    }
    // the other Styles in use, most used first
    val used = state.cardStyles.filter { it.own && it.stylesheet != setStyle }.groupingBy { it.stylesheet }.eachCount()
        .entries.sortedByDescending { it.value }.map { it.key }
    DropdownMenu(expanded = expanded, onDismissRequest = onDismiss, modifier = Modifier.testTag("add-card-menu")) {
        DropdownMenuItem(
            text = { Text("In the Set's Style" + if (setStyle.isEmpty()) "" else " (${styleLabel(styles, setStyle)})") },
            onClick = { onDismiss(); onAdd("") },
            modifier = Modifier.testTag("add-in-set-style"),
        )
        for (style in used) {
            DropdownMenuItem(
                text = { Text("In ${styleLabel(styles, style)}") },
                onClick = { onDismiss(); onAdd(style) },
                modifier = Modifier.testTag("add-in-$style"),
            )
        }
        HorizontalDivider()
        DropdownMenuItem(
            text = { Text("In another Style…") },
            onClick = { onDismiss(); picking = true },
            modifier = Modifier.testTag("add-in-other-style"),
        )
    }
    if (picking) {
        StylePickerDialog("The new Card's Style", styles, onDismiss = { picking = false }) { picked ->
            picking = false
            onAdd(if (picked.name == setStyle) "" else picked.name)
        }
    }
}

/** A full-screen Style picker (#72's list, Unsupported Styles flagged) */
@Composable
fun StylePickerDialog(title: String, styles: List<TemplateInfo>, onDismiss: () -> Unit, onPick: (TemplateInfo) -> Unit) {
    Dialog(onDismissRequest = onDismiss, properties = DialogProperties(usePlatformDefaultWidth = false)) {
        Surface(Modifier.fillMaxSize()) {
            Column(Modifier.fillMaxSize().safeDrawingPadding().padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    OutlinedButton(onClick = onDismiss) { Text("‹ Back") }
                    Text("  $title", style = MaterialTheme.typography.titleLarge)
                }
                Text("Fields the new Style doesn't have keep their values, but don't show.", style = MaterialTheme.typography.bodySmall)
                StylePicker(styles, onPick = onPick)
            }
        }
    }
}
