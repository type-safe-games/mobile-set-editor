// Mobile Set Editor - the About screen and the licences screen (#76)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import android.content.Context
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/** Where the source is, as the GPL asks */
const val SOURCE_URL = "https://github.com/type-safe-games/mobile-set-editor"

/** The app's version name, e.g. "0.1.0" */
private fun versionName(context: Context): String =
    runCatching { context.packageManager.getPackageInfo(context.packageName, 0).versionName }.getOrNull() ?: "?"

/** Name, version, what it is, the licence and its source, credits, the trademark line; and the licences */
@Composable
fun AboutScreen(onBack: () -> Unit) {
    var licences by rememberSaveable { mutableStateOf(false) }
    if (licences) {
        BackHandler { licences = false }
        LicencesScreen(onBack = { licences = false })
        return
    }
    BackHandler(onBack = onBack)
    val context = LocalContext.current
    val uri = LocalUriHandler.current
    Column(
        Modifier.fillMaxSize().safeDrawingPadding().verticalScroll(rememberScrollState()).padding(16.dp).testTag("about"),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        OutlinedButton(onClick = onBack) { Text("‹ Back") }
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            Box(Modifier.size(72.dp).clip(RoundedCornerShape(16.dp)).background(colorResource(R.color.ic_launcher_background))) {
                Image(painterResource(R.drawable.ic_launcher_foreground), contentDescription = null, Modifier.fillMaxSize())
            }
            Column {
                Text("Mobile Set Editor", style = MaterialTheme.typography.headlineSmall)
                Text("Version ${versionName(context)}", style = MaterialTheme.typography.bodyMedium, modifier = Modifier.testTag("about-version"))
            }
        }
        Text("Design your own cards and card sets, with Templates you import.", style = MaterialTheme.typography.bodyLarge)
        HorizontalDivider()
        Text(
            "Free software: you can redistribute it and modify it under the GNU General Public License, " +
                "version 3 or (at your option) any later version. It comes with no warranty.",
            style = MaterialTheme.typography.bodyMedium,
        )
        TextButton(onClick = { uri.openUri(SOURCE_URL) }, modifier = Modifier.testTag("about-source")) {
            Text("Source code: " + SOURCE_URL.removePrefix("https://"))
        }
        Text("Credits", style = MaterialTheme.typography.titleMedium)
        Text(
            "Based on MSE, the desktop card set editor by Twan van Laarhoven (twanvl) and the other MSE " +
                "developers, with later versions by haganbmj and GenevensiS.",
            style = MaterialTheme.typography.bodyMedium,
        )
        Text(
            "Not affiliated with, endorsed by or sponsored by Wizards of the Coast. " +
                "The app comes with no Templates; you bring your own.",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        OutlinedButton(onClick = { licences = true }, modifier = Modifier.testTag("about-licences")) { Text("Open source licenses") }
    }
}

/** A component on the licences screen: what assets/licences/index.tsv lists */
data class LicensedComponent(val name: String, val what: String, val licence: String, val source: String, val files: List<String>, val dir: String)

/** The components in the APK and their licences, as the build collected them (build.gradle.kts) */
fun readLicences(context: Context): Pair<List<LicensedComponent>, List<String>> {
    val components = context.assets.open("licences/index.tsv").bufferedReader().readLines().filter { it.isNotBlank() }
        .mapIndexed { i, line ->
            val f = line.split('\t')
            LicensedComponent(f[0], f[1], f[2], f[3], f[4].split(';'), "licences/$i")
        }
    val libraries = context.assets.open("licences/java-libraries.txt").bufferedReader().readLines().filter { it.isNotBlank() }
    return components to libraries
}

@Composable
fun LicencesScreen(onBack: () -> Unit) {
    val context = LocalContext.current
    val uri = LocalUriHandler.current
    val (components, libraries) = remember { readLicences(context) }
    var open by rememberSaveable { mutableStateOf<String?>(null) }
    LazyColumn(Modifier.fillMaxSize().safeDrawingPadding().padding(horizontal = 16.dp).testTag("licences")) {
        item {
            Column(Modifier.padding(vertical = 16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = onBack) { Text("‹ Back") }
                Text("Open source licenses", style = MaterialTheme.typography.headlineSmall)
                Text("Everything in this app that it didn't write itself, and the license each is used under.", style = MaterialTheme.typography.bodyMedium)
            }
        }
        items(components, key = { it.dir }) { c ->
            Column(
                Modifier.fillMaxWidth().clickable { open = if (open == c.dir) null else c.dir }.padding(vertical = 10.dp).testTag("licence-${c.name}"),
                verticalArrangement = Arrangement.spacedBy(2.dp),
            ) {
                Text(c.name, style = MaterialTheme.typography.titleMedium)
                Text(c.what, style = MaterialTheme.typography.bodySmall)
                Text(c.licence, style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.primary)
                if (open == c.dir) {
                    TextButton(onClick = { uri.openUri(c.source) }) { Text(c.source.removePrefix("https://").removePrefix("http://")) }
                    if (c.licence == "Apache-2.0") {
                        Text("The libraries:", style = MaterialTheme.typography.labelMedium)
                        Text(libraries.joinToString("\n"), style = MaterialTheme.typography.bodySmall.copy(fontFamily = FontFamily.Monospace, fontSize = 11.sp))
                    }
                    for (file in c.files) {
                        val text = remember(c.dir, file) { context.assets.open("${c.dir}/$file").bufferedReader().readText() }
                        Text(file, style = MaterialTheme.typography.labelMedium, modifier = Modifier.padding(top = 8.dp))
                        Text(
                            text,
                            style = MaterialTheme.typography.bodySmall.copy(fontFamily = FontFamily.Monospace, fontSize = 10.sp, lineHeight = 13.sp),
                            modifier = Modifier.fillMaxWidth().background(MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.5f)).padding(8.dp),
                        )
                    }
                }
            }
            HorizontalDivider()
        }
    }
}
