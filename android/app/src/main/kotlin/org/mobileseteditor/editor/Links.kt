// Mobile Set Editor - Linked cards in the swipe editor: chips that jump, link and unlink (#65)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor.editor

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.AssistChip
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import org.mobileseteditor.SetState
import org.mobileseteditor.SetViewModel
import org.mobileseteditor.core.CardLink
import org.mobileseteditor.core.LinkType

/**
 * Under the top bar: a chip for each of the Card's links, naming the relation and the linked Card
 * ("Back Face: Delver"); tapping it jumps there, its ▾ unlinks. "Link to…" links the Card to another.
 */
@Composable
fun LinkBar(model: SetViewModel, state: SetState, index: Int, onJump: (Int) -> Unit) {
    val uid = state.cardUids.getOrNull(index) ?: return
    val links = state.cardLinks.getOrElse(index) { emptyList() }
    val scope = rememberCoroutineScope()
    var linking by remember { mutableStateOf(false) }
    LazyRow(
        Modifier.fillMaxWidth().padding(horizontal = 8.dp).testTag("link-bar"),
        horizontalArrangement = Arrangement.spacedBy(6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        items(links, key = { it.uid }) { link ->
            val target = state.cardUids.indexOf(link.uid)
            val name = state.cardNames.getOrElse(target) { "?" }
            var menu by remember { mutableStateOf(false) }
            Box {
                AssistChip(
                    onClick = { if (target >= 0) onJump(target) },
                    label = { Text("${link.relation}: $name", maxLines = 1, overflow = TextOverflow.Ellipsis) },
                    trailingIcon = {
                        Text("▾", modifier = Modifier.clickable { menu = true }.padding(4.dp).testTag("link-menu-${link.uid}"))
                    },
                    modifier = Modifier.testTag("link-${link.uid}"),
                )
                DropdownMenu(expanded = menu, onDismissRequest = { menu = false }) {
                    DropdownMenuItem(
                        text = { Text("Unlink") },
                        onClick = { menu = false; scope.launch { model.unlink(uid, link.uid) } },
                        modifier = Modifier.testTag("unlink-${link.uid}"),
                    )
                }
            }
        }
        item {
            TextButton(onClick = { linking = true }, modifier = Modifier.testTag("link-to")) { Text("Link to…") }
        }
    }
    if (linking) LinkDialog(model, state, uid, links, onDone = { linking = false })
}

/** Pick a Card and a link type (or two relations of your own), and link */
@Composable
private fun LinkDialog(model: SetViewModel, state: SetState, uid: String, links: List<CardLink>, onDone: () -> Unit) {
    val scope = rememberCoroutineScope()
    var types by remember { mutableStateOf<List<LinkType>>(emptyList()) }
    LaunchedEffect(Unit) { types = model.linkTypes() }
    // cards it isn't linked to yet
    val candidates = state.cardUids.indices.filter { state.cardUids[it] != uid && links.none { l -> l.uid == state.cardUids[it] } }
    var other by remember { mutableStateOf<String?>(null) }
    // the chosen type (types.size means "custom"), and whether this Card takes the type's second relation
    var type by remember { mutableIntStateOf(0) }
    var swapped by remember { mutableStateOf(false) }
    var ownRelation by remember { mutableStateOf("") }
    var otherRelation by remember { mutableStateOf("") }
    val custom = type >= types.size
    val (mine, theirs) = when {
        custom -> ownRelation to otherRelation
        swapped -> types[type].linked to types[type].selected
        else -> types[type].selected to types[type].linked
    }
    AlertDialog(
        onDismissRequest = onDone,
        title = { Text("Link to a Card") },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Column(Modifier.fillMaxWidth().testTag("link-cards")) {
                    for (i in candidates) {
                        val u = state.cardUids[i]
                        Row(Modifier.fillMaxWidth().clickable { other = u }.testTag("link-card-$u"), verticalAlignment = Alignment.CenterVertically) {
                            RadioButton(selected = other == u, onClick = { other = u })
                            Text("${i + 1}. ${state.cardNames.getOrElse(i) { "" }}", maxLines = 1, overflow = TextOverflow.Ellipsis)
                        }
                    }
                }
                Text("This Card is:", style = MaterialTheme.typography.labelLarge)
                for ((t, lt) in types.withIndex()) {
                    for (swap in listOf(false, true)) {
                        if (swap && lt.selected == lt.linked) continue
                        val label = if (swap) "${lt.linked} (the other: ${lt.selected})" else "${lt.selected} (the other: ${lt.linked})"
                        Row(
                            Modifier.fillMaxWidth().clickable { type = t; swapped = swap }.testTag("link-type-$t-$swap"),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            RadioButton(selected = type == t && swapped == swap, onClick = { type = t; swapped = swap })
                            Text(label, style = MaterialTheme.typography.bodyMedium)
                        }
                    }
                }
                Row(Modifier.fillMaxWidth().clickable { type = types.size }, verticalAlignment = Alignment.CenterVertically) {
                    RadioButton(selected = custom, onClick = { type = types.size })
                    Text("Other relations", style = MaterialTheme.typography.bodyMedium)
                }
                if (custom) {
                    OutlinedTextField(ownRelation, { ownRelation = it }, label = { Text("This Card is") }, singleLine = true)
                    OutlinedTextField(otherRelation, { otherRelation = it }, label = { Text("The other Card is") }, singleLine = true)
                }
            }
        },
        confirmButton = {
            TextButton(
                onClick = {
                    val o = other ?: return@TextButton
                    scope.launch { if (model.link(uid, o, mine, theirs)) onDone() }
                },
                enabled = other != null && mine.isNotBlank() && theirs.isNotBlank() && (custom || types.isNotEmpty()),
                modifier = Modifier.testTag("link-confirm"),
            ) { Text("Link") }
        },
        dismissButton = { TextButton(onClick = onDone) { Text("Cancel") } },
    )
}
