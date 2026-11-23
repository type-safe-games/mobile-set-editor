// Mobile Set Editor - a field's value shown as a field: in Set info and the Card details sheet (#84, #88)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.editor

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowDropDown
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material3.Icon
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp

/** Field types picked from a list rather than typed */
private val pickedTypes = setOf("choice", "multiple choice", "package choice", "color", "boolean")

/**
 * A read-only outlined field with the field's name as its label and its value inside, so it reads
 * as something to edit (#84). Tapping anywhere on it calls [onClick], which opens the field's editor
 * (the field sheet); it never takes the keyboard itself. A trailing ▾ marks fields picked from a
 * list, a pencil images and symbols, which open their own editors.
 */
@Composable
fun FieldBox(label: String, value: String, type: String, editable: Boolean, multiLine: Boolean, onClick: () -> Unit, modifier: Modifier = Modifier) {
    Box(modifier.fillMaxWidth().padding(vertical = 4.dp)) {
        OutlinedTextField(
            value = value,
            onValueChange = {},
            readOnly = true,
            enabled = editable,
            label = { Text(if (editable) label else "$label (not editable)", maxLines = 1, overflow = TextOverflow.Ellipsis) },
            singleLine = !multiLine,
            maxLines = if (multiLine) 3 else 1,
            trailingIcon = when (type) {
                in pickedTypes -> ({ Icon(Icons.Filled.ArrowDropDown, contentDescription = null) })
                "image", "symbol" -> ({ Icon(Icons.Filled.Edit, contentDescription = null) })
                else -> null
            },
            modifier = Modifier.fillMaxWidth(),
        )
        // the whole field opens the editor; the text field underneath never gets focus
        if (editable) {
            Box(
                Modifier.matchParentSize()
                    .clickable(role = Role.Button, onClick = onClick)
                    .semantics { contentDescription = "Edit $label" },
            )
        }
    }
}
