// Mobile Set Editor - the field sheet's editors for every field that isn't text: each change is one setValue (#63)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.editor

import android.net.Uri
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.Button
import androidx.compose.material3.Checkbox
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import kotlin.math.roundToInt
import kotlinx.coroutines.launch
import io.github.type_safe_games.mobileseteditor.SetViewModel
import io.github.type_safe_games.mobileseteditor.core.ActionResult
import io.github.type_safe_games.mobileseteditor.core.Core
import io.github.type_safe_games.mobileseteditor.core.FieldInfo
import io.github.type_safe_games.mobileseteditor.core.TargetKind

/** Field types the sheet edits with [ValueEditor] */
val VALUE_EDITOR_TYPES = setOf("choice", "package choice", "multiple choice", "slider", "boolean", "color", "image", "symbol")

/**
 * How the image editor asks for an image: by default the system photo picker. Tests provide one
 * that answers with a file of theirs.
 */
fun interface ImagePicker {
    fun pick(onPicked: (Uri?) -> Unit)
}

val LocalImagePicker = compositionLocalOf<ImagePicker?> { null }

/**
 * The editor of a field that is replaced whole ([VALUE_EDITOR_TYPES]): every change is one
 * setValue, so one undo step, and re-renders the Card at once. [aspect] is the field's width /
 * height on the Card (for cropping images). [revision] changes after Undo and Redo.
 */
@Composable
fun ValueEditor(model: SetViewModel, session: Long, uid: String, info: FieldInfo, aspect: Float, revision: Int, kind: TargetKind = TargetKind.Card) {
    val scope = rememberCoroutineScope()
    var value by remember(uid, info.name, kind) { mutableStateOf<String?>(null) }
    LaunchedEffect(uid, info.name, kind, revision) {
        value = Core.getValue(session, kind, uid, info.name).value
    }
    val apply: suspend (String) -> Unit = { new ->
        value = new
        val result: ActionResult = Core.setValue(session, kind, uid, info.name, new)
        model.edited(result)
        value = Core.getValue(session, kind, uid, info.name).value
    }
    val set: (String) -> Unit = { new -> scope.launch { apply(new) } }
    val current = value ?: return
    when (info.type) {
        "choice", "package choice" -> ChoiceEditor(info, current, set)
        "multiple choice" -> MultipleChoiceEditor(info, current, set)
        "slider" -> SliderEditor(info, current, set)
        "boolean" -> BooleanEditor(info, current, set)
        "color" -> ColorEditor(info, current, set)
        "image" -> ImageEditor(current, aspect, apply)
        "symbol" -> SymbolFileEditor(current, apply)
    }
}

@Composable
private fun ChoiceEditor(info: FieldInfo, value: String, set: (String) -> Unit) {
    val list = rememberLazyListState(initialFirstVisibleItemIndex = maxOf(0, info.choices.indexOf(value)))
    LazyColumn(Modifier.fillMaxWidth().heightIn(max = 240.dp).testTag("choices"), state = list) {
        items(info.choices) { choice ->
            Row(
                Modifier.fillMaxWidth().clickable { if (choice != value) set(choice) }.testTag("choice-$choice"),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                RadioButton(selected = choice == value, onClick = { if (choice != value) set(choice) })
                Text(choice.ifEmpty { "(none)" }, style = MaterialTheme.typography.bodyMedium)
            }
        }
    }
}

/** The value of a multiple choice field: its choice names, separated by ", " */
object MultipleChoice {
    fun parse(value: String): Set<String> = value.split(",").map { it.trim() }.filter { it.isNotEmpty() }.toSet()

    /** [value] with [choice] turned on or off, in the field's order of choices */
    fun toggle(choices: List<String>, value: String, choice: String): String {
        val on = parse(value).toMutableSet()
        if (!on.remove(choice)) on += choice
        return choices.filter { it in on }.joinToString(", ")
    }
}

@Composable
private fun MultipleChoiceEditor(info: FieldInfo, value: String, set: (String) -> Unit) {
    val on = MultipleChoice.parse(value)
    LazyColumn(Modifier.fillMaxWidth().heightIn(max = 240.dp).testTag("choices")) {
        items(info.choices) { choice ->
            val toggle = { set(MultipleChoice.toggle(info.choices, value, choice)) }
            Row(
                Modifier.fillMaxWidth().clickable(onClick = toggle).testTag("check-$choice"),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Checkbox(checked = choice in on, onCheckedChange = { toggle() })
                Text(choice, style = MaterialTheme.typography.bodyMedium)
            }
        }
    }
}

/** A yes/no field: a switch (its choices are its two values, "yes" first, as the core lists them) */
@Composable
private fun BooleanEditor(info: FieldInfo, value: String, set: (String) -> Unit) {
    val yes = info.choices.getOrElse(0) { "yes" }
    val no = info.choices.getOrElse(1) { "no" }
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        Switch(checked = value == yes, onCheckedChange = { set(if (it) yes else no) }, modifier = Modifier.testTag("boolean"))
        Text(value, style = MaterialTheme.typography.bodyLarge)
    }
}

/** A slider over the field's choices (numbers, for a slider field), with − and + */
@Composable
private fun SliderEditor(info: FieldInfo, value: String, set: (String) -> Unit) {
    val choices = info.choices
    if (choices.isEmpty()) return
    val index = choices.indexOf(value).coerceAtLeast(0)
    var dragged by remember(value) { mutableFloatStateOf(index.toFloat()) }
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        OutlinedButton(onClick = { set(choices[index - 1]) }, enabled = index > 0, modifier = Modifier.testTag("slider-minus")) { Text("−") }
        Text(choices[dragged.roundToInt().coerceIn(choices.indices)], style = MaterialTheme.typography.titleMedium, modifier = Modifier.testTag("slider-value"))
        OutlinedButton(onClick = { set(choices[index + 1]) }, enabled = index < choices.size - 1, modifier = Modifier.testTag("slider-plus")) { Text("+") }
    }
    if (choices.size > 1) {
        Slider(
            value = dragged,
            onValueChange = { dragged = it },
            onValueChangeFinished = {
                val i = dragged.roundToInt().coerceIn(choices.indices)
                if (i != index) set(choices[i])
            },
            valueRange = 0f..(choices.size - 1).toFloat(),
            steps = maxOf(0, choices.size - 2),
            modifier = Modifier.fillMaxWidth().testTag("slider"),
        )
    }
}

/** Colors as the core writes them: "rgb(r,g,b)", "rgba(r,g,b,a)" or "#rrggbb" */
object CoreColor {
    fun parse(text: String): Color? {
        val t = text.trim()
        if (t.startsWith("#") && t.length == 7) {
            val v = t.substring(1).toIntOrNull(16) ?: return null
            return Color(0xFF000000.toInt() or v)
        }
        val inner = t.substringAfter("(", "").substringBefore(")", "")
        val parts = inner.split(",").map { it.trim().toIntOrNull() ?: return null }
        return when {
            t.startsWith("rgba(") && parts.size == 4 -> Color(parts[0], parts[1], parts[2], parts[3])
            t.startsWith("rgb(") && parts.size == 3 -> Color(parts[0], parts[1], parts[2])
            else -> null
        }
    }

    fun format(r: Int, g: Int, b: Int) = "rgb($r,$g,$b)"
}

@Composable
private fun ColorEditor(info: FieldInfo, value: String, set: (String) -> Unit) {
    val current = CoreColor.parse(value)
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            items(info.colors) { preset ->
                val color = CoreColor.parse(preset.color) ?: Color.Gray
                val chosen = current != null && CoreColor.parse(preset.color) == current
                Column(
                    Modifier.clickable { set(preset.color) }.testTag("color-${preset.name}"),
                    horizontalAlignment = Alignment.CenterHorizontally,
                ) {
                    Box(
                        Modifier.size(36.dp).background(color, CircleShape)
                            .border(if (chosen) 3.dp else 1.dp, if (chosen) MaterialTheme.colorScheme.primary else Color.Gray, CircleShape),
                    )
                    Text(preset.name, style = MaterialTheme.typography.labelSmall, maxLines = 1)
                }
            }
        }
        if (info.allowCustom) CustomColor(current ?: Color.Black, set)
    }
}

/** Red, green and blue sliders, applied with one button (one undo step, not one per drag) */
@Composable
private fun CustomColor(initial: Color, set: (String) -> Unit) {
    var r by remember(initial) { mutableFloatStateOf(initial.red * 255) }
    var g by remember(initial) { mutableFloatStateOf(initial.green * 255) }
    var b by remember(initial) { mutableFloatStateOf(initial.blue * 255) }
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Column(Modifier.weight(1f)) {
            for ((name, get, put) in listOf(
                Triple("R", { r }, { v: Float -> r = v }),
                Triple("G", { g }, { v: Float -> g = v }),
                Triple("B", { b }, { v: Float -> b = v }),
            )) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(name, modifier = Modifier.padding(end = 4.dp))
                    Slider(value = get(), onValueChange = put, valueRange = 0f..255f, modifier = Modifier.weight(1f).testTag("custom-$name"))
                }
            }
        }
        Box(Modifier.size(40.dp).background(Color(r.roundToInt(), g.roundToInt(), b.roundToInt()), CircleShape).border(1.dp, Color.Gray, CircleShape))
        Button(onClick = { set(CoreColor.format(r.roundToInt(), g.roundToInt(), b.roundToInt())) }, modifier = Modifier.testTag("custom-apply")) {
            Text("Use")
        }
    }
}

@Composable
private fun ImageEditor(value: String, aspect: Float, apply: suspend (String) -> Unit) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var picked by remember { mutableStateOf<Uri?>(null) }
    val photoPicker = rememberLauncherForActivityResult(ActivityResultContracts.PickVisualMedia()) { picked = it }
    val picker = LocalImagePicker.current ?: ImagePicker { _ ->
        photoPicker.launch(PickVisualMediaRequest(ActivityResultContracts.PickVisualMedia.ImageOnly))
    }
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
        Button(onClick = { picker.pick { picked = it } }, modifier = Modifier.testTag("pick-image")) {
            Text(if (value.isEmpty()) "Pick image" else "Replace image")
        }
        TextButton(onClick = { scope.launch { apply("") } }, enabled = value.isNotEmpty(), modifier = Modifier.testTag("remove-image")) { Text("Remove image") }
    }
    picked?.let { uri ->
        CropDialog(
            uri, aspect,
            onCropped = { file ->
                picked = null
                scope.launch {
                    apply(file.absolutePath)
                    // the core has copied it into the Set
                    file.delete()
                }
            },
            onCancel = { picked = null },
            cacheDir = context.cacheDir,
        )
    }
}

/** The .mse-symbol files inside the installed Templates (e.g. a Style's rarity symbols) */
fun templateSymbols(templates: File): List<File> =
    templates.walkTopDown().maxDepth(3).filter { it.isFile && it.name.endsWith(".mse-symbol") }.sortedBy { it.path }.toList()

/**
 * A symbol field (the Set symbol): a .mse-symbol file picked with the system picker, or one from
 * the installed Templates, copied into the Set; or Remove. (Drawing symbols is post-MVP: the
 * Cards show the result.)
 */
@Composable
private fun SymbolFileEditor(value: String, apply: suspend (String) -> Unit) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var fromTemplates by remember { mutableStateOf(false) }
    val pick = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) scope.launch {
            // a file of ours to hand to the core, which copies it into the Set
            val file = File(context.cacheDir, "picked-${System.nanoTime()}.mse-symbol")
            val copied = withContext(Dispatchers.IO) {
                runCatching { context.contentResolver.openInputStream(uri)?.use { i -> file.outputStream().use { i.copyTo(it) } } != null }.getOrDefault(false)
            }
            if (copied) apply(file.path)
            file.delete()
        }
    }
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(if (value.isEmpty()) "No symbol" else "A symbol is set", style = MaterialTheme.typography.bodyMedium, modifier = Modifier.testTag("symbol-state"))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { pick.launch(arrayOf("*/*")) }, modifier = Modifier.testTag("symbol-file")) { Text("From a file…") }
            OutlinedButton(onClick = { fromTemplates = true }, modifier = Modifier.testTag("symbol-templates")) { Text("From a Template…") }
            TextButton(onClick = { scope.launch { apply("") } }, enabled = value.isNotEmpty(), modifier = Modifier.testTag("symbol-remove")) { Text("Remove") }
        }
    }
    if (fromTemplates) {
        val templates = remember { io.github.type_safe_games.mobileseteditor.core.Stores.templates(context) }
        val files by androidx.compose.runtime.produceState<List<File>?>(null) {
            val loaded = withContext(Dispatchers.IO) { templateSymbols(templates) }
            // written on the main thread, between compositions: written from the IO thread, the
            // update could be lost to the dialog composing meanwhile (#92)
            withContext(Dispatchers.Main.immediate) { this@produceState.value = loaded }
        }
        androidx.compose.material3.AlertDialog(
            onDismissRequest = { fromTemplates = false },
            title = { Text("A symbol from a Template") },
            text = {
                val list = files
                if (list == null) androidx.compose.material3.CircularProgressIndicator()
                else if (list.isEmpty()) Text("The installed Templates have no .mse-symbol files.")
                else LazyColumn(Modifier.heightIn(max = 360.dp)) {
                    items(list) { f ->
                        Column(
                            Modifier.fillMaxWidth().clickable { fromTemplates = false; scope.launch { apply(f.path) } }
                                .padding(vertical = 6.dp).testTag("template-symbol-${f.name}"),
                        ) {
                            Text(f.nameWithoutExtension, style = MaterialTheme.typography.bodyLarge)
                            Text(f.parentFile?.name.orEmpty(), style = MaterialTheme.typography.bodySmall)
                        }
                    }
                }
            },
            confirmButton = { TextButton(onClick = { fromTemplates = false }) { Text("Cancel") } },
        )
    }
}
