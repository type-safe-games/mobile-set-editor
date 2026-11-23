// Mobile Set Editor - the app's entry point
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import android.os.Bundle
import android.text.format.Formatter
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.viewModels
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.foundation.lazy.grid.rememberLazyGridState
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.viewModelScope
import androidx.lifecycle.viewmodel.compose.viewModel
import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.launch
import io.github.type_safe_games.mobileseteditor.core.Outcome
import io.github.type_safe_games.mobileseteditor.core.TemplateInfo
import io.github.type_safe_games.mobileseteditor.core.Verdict
import io.github.type_safe_games.mobileseteditor.editor.CardEditorScreen

/** For now two screens: the Templates, and an open Set's Cards; the real UI comes with M4 */
class MainActivity : ComponentActivity() {
    private val setModel: SetViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // after the process was killed: back to the Set that was open, from its Working copy
        setModel.restore()
        setContent {
            MaterialTheme {
                Surface(Modifier.fillMaxSize()) {
                    App(setModel)
                }
            }
        }
    }

    override fun onStop() {
        super.onStop()
        // Android may kill the app from here on without warning: autosave now
        lifecycleScope.launch(NonCancellable) { setModel.flushNow() }
    }
}

private enum class Screen { Start, Set, Card, Templates, NewSet }

@Composable
fun App(setModel: SetViewModel) {
    val state by setModel.state.collectAsState()
    var screen by rememberSaveable { mutableStateOf(Screen.Start) }
    // the grid's scroll position outlives a visit to a Card
    val grid = rememberLazyGridState()
    // an open Set (also one restored after process death) shows its home
    LaunchedEffect(state.session, state.opening) {
        if ((state.session != 0L || state.opening) && screen == Screen.Start) screen = Screen.Set
    }
    val openSet = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) {
            setModel.open(uri)
            screen = Screen.Set
        }
    }
    val saveAs = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/octet-stream")) { uri ->
        if (uri != null) setModel.viewModelScope.launch { setModel.saveAs(uri) }
    }
    // (.mse-set files have no registered type)
    val pickSet = { openSet.launch(arrayOf("*/*")) }
    when (screen) {
        Screen.Start -> StartScreen(
            onOpenSet = pickSet,
            onOpenCopy = { setModel.openWorkingCopy(it); screen = Screen.Set },
            onTemplates = { screen = Screen.Templates },
            onNewSet = { screen = Screen.NewSet },
        )
        Screen.NewSet -> NewSetScreen(
            onCreate = { game, style ->
                setModel.newSet(game, style)
                screen = Screen.Set
            },
            onTemplates = { screen = Screen.Templates },
            onBack = { screen = Screen.Start },
        )
        Screen.Set -> SetHomeScreen(
            setModel,
            grid,
            onOpenCard = { index ->
                setModel.go(index)
                setModel.showCard()
                screen = Screen.Card
            },
            onSaveAs = { saveAs.launch((state.workingCopy?.name ?: "set").removeSuffix(".mse-set") + ".mse-set") },
            onClosed = { screen = Screen.Start },
        )
        Screen.Card -> CardEditorScreen(setModel, onBack = { screen = Screen.Set })
        Screen.Templates -> {
            BackHandler { screen = Screen.Start }
            TemplatesScreen(
                onOpenSet = pickSet,
                onShowSet = { screen = Screen.Set },
                hasSet = state.session != 0L,
            )
        }
    }
}
