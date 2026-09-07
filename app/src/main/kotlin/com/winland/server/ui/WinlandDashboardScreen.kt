package com.winland.server.ui

import android.content.Intent
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.PressInteraction
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.runtime.rememberCoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.animation.animateColorAsState
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.Download
import androidx.compose.material.icons.filled.FormatSize
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.Pause
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Terminal
import androidx.compose.material.icons.filled.Usb
import androidx.compose.material.icons.filled.DisplaySettings
import androidx.compose.material.icons.filled.DarkMode
import androidx.compose.material.icons.filled.PowerSettingsNew
import androidx.compose.material.icons.filled.TouchApp
import androidx.compose.material.icons.filled.Laptop
import androidx.compose.material.icons.filled.Mouse
import androidx.compose.material.icons.filled.LightMode
import androidx.compose.material.icons.filled.PhoneAndroid
import androidx.compose.material.icons.filled.BugReport
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.Info
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CenterAlignedTopAppBar
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.LocalTextStyle
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarDuration
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import com.winland.server.DashboardTab
import com.winland.server.ExecutionMode
import com.winland.server.ExecutionModeManager
import com.winland.server.LinuxDistro
import com.winland.server.MainViewModel
import java.util.UUID
import com.winland.server.NativeBridge
import com.winland.server.engine.ChrootInstaller
import com.winland.server.engine.ProotManager
import com.winland.server.utils.getInstalledDistros
import com.winland.server.ui.theme.ActionGreen
import com.winland.server.ui.theme.ActionBlue
import com.winland.server.ui.theme.ActionRed

// Semantic action colors: Run = green, Restart = blue, Stop = red.
// Single hues readable on both dark and light themes (stepping stone
// toward the full blue-identity redesign, where these become theme roles).
private val RunGreen = ActionGreen
private val RestartBlue = ActionBlue
private val StopRed = ActionRed

data class WinlandDashboardActions(
    val onRequestUsb: () -> Unit,
    val onDistroInstall: (LinuxDistro) -> Unit,
    val onDistroSetup: (String) -> Unit,
    val onDistroRun: (String) -> Unit,
    val onDistroStop: () -> Unit,
    val onDistroRestart: () -> Unit,
    val onShowMessage: (String, Boolean) -> Unit
)

private data class ResolutionOption(
    val label: String,
    val scale: Float
)

private enum class ConfirmAction { STOP, RESTART }

private data class TerminalSessionTab(
    val id: String = UUID.randomUUID().toString(),
    val label: MutableState<String>,
    val distroId: String,
    val isCustomName: MutableState<Boolean> = mutableStateOf(false),
    val ctrlActive: MutableState<Boolean> = mutableStateOf(false),
    val altActive: MutableState<Boolean> = mutableStateOf(false)
)

@Composable
fun WinlandDashboardScreen(
    distros: List<LinuxDistro>,
    viewModel: MainViewModel,
    actions: WinlandDashboardActions
) {
    val appContext = LocalContext.current
    val clipboard = LocalClipboardManager.current
    val haptics = LocalHapticFeedback.current
    val themeSettings by viewModel.themeSettings.collectAsState()
    val activeUiOperation by viewModel.activeUiOperation.collectAsState()
    val selectedTab by viewModel.selectedTab.collectAsState()
    val activeDistroId by viewModel.activeDistroId.collectAsState()
    val logsPaused by viewModel.logsPaused.collectAsState()
    val logSearchQuery by viewModel.logSearchQuery.collectAsState()
    val displayedLogs by viewModel.filteredLogs.collectAsState()
    val snackbarHostState = remember { SnackbarHostState() }
    LaunchedEffect(Unit) {
        viewModel.messages.collect { (message, isError) ->
            snackbarHostState.showSnackbar(
                message = message,
                withDismissAction = true,
                duration = if (isError) SnackbarDuration.Long else SnackbarDuration.Short
            )
        }
    }
    LaunchedEffect(distros) {
        viewModel.ensureDistroStates(distros.map { it.id })
    }

    LaunchedEffect(Unit) {
        ChrootInstaller.logFlow.collect { line ->
            if (!logsPaused) {
                viewModel.appendLogLine(line)
            }
        }
    }

    val activeOperationText = when {
        activeUiOperation == MainViewModel.UiOperation.DOWNLOAD -> "Download + Extract running"
        activeUiOperation == MainViewModel.UiOperation.SETUP -> "Setup running"
        activeUiOperation == MainViewModel.UiOperation.RUN -> "Run launching"
        else -> null
    }

    val embeddedTerminal = remember { EmbeddedTerminal(appContext) }
    val keyboardController = LocalSoftwareKeyboardController.current

    var sessionTabs by remember {
        mutableStateOf(
            listOf(TerminalSessionTab(label = mutableStateOf("Session 1"), distroId = activeDistroId ?: "ubuntu"))
        )
    }
    var activeSessionId by remember { mutableStateOf(sessionTabs.first().id) }
    val activeSession = remember(sessionTabs, activeSessionId) {
        sessionTabs.first { it.id == activeSessionId }
    }

    fun addSession() {
        val newTab = TerminalSessionTab(
            label = mutableStateOf("Session ${sessionTabs.size + 1}"),
            distroId = activeSession.distroId
        )
        sessionTabs = sessionTabs + newTab
        activeSessionId = newTab.id
    }

    fun closeSession(tabId: String) {
        if (sessionTabs.size <= 1) return
        val closedIndex = sessionTabs.indexOfFirst { it.id == tabId }
        sessionTabs = sessionTabs.filter { it.id != tabId }
        embeddedTerminal.finishSession(tabId)
        if (activeSessionId == tabId) {
            val newIndex = closedIndex.coerceAtMost(sessionTabs.size - 1)
            activeSessionId = sessionTabs[newIndex].id
        }
        var counter = 0
        sessionTabs = sessionTabs.map { tab ->
            if (!tab.isCustomName.value) tab.copy(label = mutableStateOf("Session ${++counter}"), ctrlActive = tab.ctrlActive, altActive = tab.altActive, isCustomName = tab.isCustomName) else tab
        }
    }

    LaunchedEffect(selectedTab) {
        keyboardController?.hide()
    }

    Scaffold(
        topBar = {
            if (selectedTab != DashboardTab.Terminal) {
                ProfessionalTopBar(activeOperationText = activeOperationText)
            }
        },
        snackbarHost = {
            SnackbarHost(
                hostState = snackbarHostState,
                modifier = Modifier.padding(bottom = 84.dp)
            )
        },
        contentWindowInsets = WindowInsets.systemBars.only(WindowInsetsSides.Top),
    ) { innerPadding ->
        BoxWithConstraints(modifier = Modifier.fillMaxSize().padding(innerPadding)) {
            val isWide = maxWidth >= 1000.dp

            Box(modifier = Modifier.fillMaxSize()) {
                if (selectedTab == DashboardTab.Terminal) {
                    val imeVisible = WindowInsets.ime.getBottom(LocalDensity.current) > 0
                    val termDark = if (themeSettings.followSystemTheme) isSystemInDarkTheme() else themeSettings.darkModeEnabled

                    LaunchedEffect(Unit) {
                        embeddedTerminal.onBarStateChanged = { sessionId, c, a ->
                            sessionTabs.find { it.id == sessionId }?.let { tab ->
                                tab.ctrlActive.value = c
                                tab.altActive.value = a
                            }
                        }
                    }

                    Column(
                        modifier = Modifier
                            .fillMaxSize()
                            .background(if (termDark) Color(0xFF282C34) else Color(0xFFFAFAFC))
                            .imePadding()
                            .padding(bottom = if (imeVisible) 0.dp else 84.dp)
                    ) {
                        var showRenameDialog by remember { mutableStateOf<TerminalSessionTab?>(null) }

                        fun renameSession(tabId: String, newLabel: String) {
                            if (newLabel.isBlank()) return
                            val tab = sessionTabs.first { it.id == tabId }
                            tab.label.value = newLabel.trim()
                            tab.isCustomName.value = true
                        }

                        showRenameDialog?.let { tab ->
                            var textValue by remember(tab.id) { mutableStateOf(tab.label.value) }
                            AlertDialog(
                                onDismissRequest = { showRenameDialog = null },
                                title = { Text("Rename Session") },
                                text = {
                                    OutlinedTextField(
                                        value = textValue,
                                        onValueChange = { textValue = it },
                                        singleLine = true
                                    )
                                },
                                confirmButton = {
                                    TextButton(onClick = {
                                        renameSession(tab.id, textValue)
                                        showRenameDialog = null
                                    }) {
                                        Text("Rename")
                                    }
                                },
                                dismissButton = {
                                    TextButton(onClick = { showRenameDialog = null }) {
                                        Text("Cancel")
                                    }
                                }
                            )
                        }

                        SessionSwitcherBar(
                            sessionTabs = sessionTabs,
                            activeSessionId = activeSessionId,
                            onSelectTab = { activeSessionId = it },
                            onCloseTab = { closeSession(it) },
                            onAddTab = { addSession() },
                            onRenameRequest = { showRenameDialog = it },
                            modifier = Modifier.fillMaxWidth()
                        )
                        Box(modifier = Modifier.weight(1f)) {
                            AndroidView(
                                factory = { _ ->
                                    embeddedTerminal.forceDarkTheme = termDark
                                    val view = embeddedTerminal.createView()
                                    embeddedTerminal.startSession(activeSessionId, activeSession.distroId)
                                    view
                                },
                                update = { view ->
                                    embeddedTerminal.refreshTheme(termDark)
                                    embeddedTerminal.attachSession(view, activeSessionId, activeSession.distroId)
                                },
                                modifier = Modifier.fillMaxSize()
                            )
                        }
                        if (imeVisible) {
                            TerminalExtraKeysBar(
                                ctrlActive = activeSession.ctrlActive.value,
                                altActive = activeSession.altActive.value,
                                onCtrlToggle = {
                                    val newVal = !activeSession.ctrlActive.value
                                    activeSession.ctrlActive.value = newVal
                                    embeddedTerminal.setModifierState(activeSessionId, newVal, activeSession.altActive.value)
                                },
                                onAltToggle = {
                                    val newVal = !activeSession.altActive.value
                                    activeSession.altActive.value = newVal
                                    embeddedTerminal.setModifierState(activeSessionId, activeSession.ctrlActive.value, newVal)
                                },
                                onKey = { key ->
                                    embeddedTerminal.sendSpecialKey(key)
                                },
                                modifier = Modifier.fillMaxWidth()
                            )
                        }
                    }
                } else {
                    Column(
                        modifier = Modifier
                            .fillMaxSize()
                            .padding(horizontal = 14.dp)
                            .padding(bottom = 84.dp)
                    ) {
                        activeOperationText?.let { op ->
                            GlassCard(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .padding(top = 10.dp, bottom = 6.dp),
                                shape = RoundedCornerShape(12.dp)
                            ) {
                                Text(
                                    text = "Active operation: $op. Buttons are locked until completion.",
                                    modifier = Modifier.padding(horizontal = 12.dp, vertical = 10.dp),
                                    color = MaterialTheme.colorScheme.onTertiaryContainer,
                                    style = MaterialTheme.typography.labelLarge
                                )
                            }
                        }

                        Spacer(Modifier.height(6.dp))

                        if (selectedTab == DashboardTab.Home) {
                            if (isWide) {
                                Row(
                                    modifier = Modifier.fillMaxWidth().weight(1f),
                                    horizontalArrangement = Arrangement.spacedBy(12.dp)
                                ) {
                                    LogPanel(
                                        displayedLogs = displayedLogs,
                                        logSearchQuery = logSearchQuery,
                                        logsPaused = logsPaused,
                                        onCopyLogs = { clipboard.setText(AnnotatedString(displayedLogs.joinToString("\n"))) },
                                        onToggleLogsPaused = { viewModel.toggleLogsPaused() },
                                        onSearchChange = { viewModel.setLogSearchQuery(it) },
                                        modifier = Modifier.weight(1.4f)
                                    )
                                    LazyColumn(
                                        modifier = Modifier.weight(1f),
                                        verticalArrangement = Arrangement.spacedBy(12.dp)
                                    ) {
                                        items(distros) { distro ->
                                            DistroCard(distro, viewModel, actions)
                                        }
                                    }
                                }
                            } else {
                                LazyColumn(
                                    modifier = Modifier.fillMaxWidth().weight(1f),
                                    verticalArrangement = Arrangement.spacedBy(12.dp)
                                ) {
                                    items(distros) { distro ->
                                        DistroCard(distro, viewModel, actions)
                                    }
                                    item {
                                        LogPanel(
                                            displayedLogs = displayedLogs,
                                            logSearchQuery = logSearchQuery,
                                            logsPaused = logsPaused,
                                            onCopyLogs = { clipboard.setText(AnnotatedString(displayedLogs.joinToString("\n"))) },
                                            onToggleLogsPaused = { viewModel.toggleLogsPaused() },
                                            onSearchChange = { viewModel.setLogSearchQuery(it) },
                                            modifier = Modifier.fillMaxWidth().height(280.dp)
                                        )
                                    }
                                }
                            }
                        } else {
                            SettingsPanel(
                                distros = distros,
                                viewModel = viewModel,
                                followSystemTheme = themeSettings.followSystemTheme,
                                darkModeEnabled = themeSettings.darkModeEnabled,
                                dynamicColorEnabled = themeSettings.dynamicColorEnabled,
                                onDynamicColorChanged = { viewModel.updateDynamicColor(it) },
                                onThemeModeChanged = { followSystem, darkEnabled ->
                                    viewModel.updateThemeMode(followSystem, darkEnabled)
                                },
                                onResolutionApplied = { resolution ->
                                    actions.onShowMessage("Resolution updated to $resolution", false)
                                },
                                onRequestUsb = actions.onRequestUsb,
                                onStopChroot = actions.onDistroStop,
                                onRestartChroot = actions.onDistroRestart,
                                activeDistroId = activeDistroId,
                                controlsEnabled = activeUiOperation == null,
                                modifier = Modifier.fillMaxWidth().weight(1f)
                            )
                        }
                    }
                }

                ModernNavigationBar(
                    selectedTab = selectedTab,
                    onTabSelected = {
                        haptics.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                        viewModel.setSelectedTab(it)
                    },
                    modifier = Modifier.align(Alignment.BottomCenter)
                )
            }
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun LogPanel(
    displayedLogs: List<String>,
    logSearchQuery: String,
    logsPaused: Boolean,
    onCopyLogs: () -> Unit,
    onToggleLogsPaused: () -> Unit,
    onSearchChange: (String) -> Unit,
    modifier: Modifier = Modifier
) {
    GlassCard(
        modifier = modifier,
        shape = RoundedCornerShape(14.dp)
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(10.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text("Live Logs", fontWeight = FontWeight.SemiBold)
                Spacer(Modifier.weight(1f))
                IconButton(onClick = onCopyLogs) {
                    Icon(Icons.Default.ContentCopy, "Copy logs")
                }
                IconButton(onClick = onToggleLogsPaused) {
                    Icon(if (logsPaused) Icons.Default.PlayArrow else Icons.Default.Pause, "Pause logs")
                }
            }

            OutlinedTextField(
                value = logSearchQuery,
                onValueChange = onSearchChange,
                modifier = Modifier.fillMaxWidth(),
                singleLine = true,
                textStyle = LocalTextStyle.current.copy(fontSize = 12.sp),
                placeholder = { Text("Filter logs...", fontSize = 12.sp) },
                trailingIcon = { Icon(Icons.Default.Search, "Search logs") }
            )

            val listState = rememberLazyListState()
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(top = 8.dp)
                    .clip(RoundedCornerShape(14.dp))
                    .background(MaterialTheme.colorScheme.surface)
                    .border(1.dp, MaterialTheme.colorScheme.outlineVariant, RoundedCornerShape(14.dp))
            ) {
                if (displayedLogs.isEmpty()) {
                    EmptyState(
                        icon = Icons.Default.Terminal,
                        title = "No logs yet",
                        body = "Run an operation (Install, Setup, Run) and its output will appear here."
                    )
                } else {
                    SelectionContainer {
                        LazyColumn(
                            state = listState,
                            modifier = Modifier.fillMaxSize().padding(horizontal = 10.dp, vertical = 6.dp),
                            verticalArrangement = Arrangement.spacedBy(0.dp)
                        ) {
                            items(
                                count = displayedLogs.size,
                                key = { index -> index }
                            ) { index ->
                                val line = displayedLogs[index]
                                Text(
                                    text = line,
                                    color = MaterialTheme.colorScheme.onSurface,
                                    fontFamily = FontFamily.Monospace,
                                    fontSize = 12.sp,
                                    lineHeight = 16.sp,
                                    modifier = Modifier.animateItemPlacement()
                                )
                            }
                        }
                    }
                    LaunchedEffect(displayedLogs.size) {
                        if (displayedLogs.isNotEmpty()) {
                            listState.animateScrollToItem(displayedLogs.size - 1)
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun SettingsPanel(
    distros: List<LinuxDistro>,
    viewModel: MainViewModel,
    followSystemTheme: Boolean,
    darkModeEnabled: Boolean,
    dynamicColorEnabled: Boolean,
    onThemeModeChanged: (Boolean, Boolean) -> Unit,
    onDynamicColorChanged: (Boolean) -> Unit,
    onResolutionApplied: (String) -> Unit,
    onRequestUsb: () -> Unit,
    onStopChroot: () -> Unit,
    onRestartChroot: () -> Unit,
    activeDistroId: String?,
    controlsEnabled: Boolean,
    modifier: Modifier = Modifier
) {
    val appContext = LocalContext.current
    val displayInfo by viewModel.displayInfo.collectAsState()
    var localFollowSystem by remember { mutableStateOf(followSystemTheme) }
    var localDarkMode by remember { mutableStateOf(darkModeEnabled) }
    val installedDistros = remember { appContext.getInstalledDistros() }

    LaunchedEffect(followSystemTheme, darkModeEnabled) {
        localFollowSystem = followSystemTheme
        localDarkMode = darkModeEnabled
    }

    val resolution1080p by remember {
        derivedStateOf {
            ResolutionOption("1080p", 1.0f)
        }
    }
    val resolution720p by remember {
        derivedStateOf {
            ResolutionOption("720p", 1.5f)
        }
    }
    val resolution540p by remember {
        derivedStateOf {
            ResolutionOption("540p", 2.0f)
        }
    }
    var selectedResolutionLabel by rememberSaveable { mutableStateOf(resolution1080p.label) }
    val selectedResolution = when (selectedResolutionLabel) {
        resolution720p.label -> resolution720p
        resolution540p.label -> resolution540p
        else -> resolution1080p
    }

    val scroll = rememberScrollState()
    var confirmAction by remember { mutableStateOf<ConfirmAction?>(null) }

    Column(
        modifier = modifier
            .verticalScroll(scroll)
            .padding(horizontal = 4.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        Text(
            text = "Settings",
            style = MaterialTheme.typography.headlineMedium,
            fontWeight = FontWeight.SemiBold,
            modifier = Modifier.padding(top = 6.dp)
        )

        GlassCard {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                SectionHeader(icon = Icons.Default.Home, title = "Default Distro")
                HorizontalDivider(modifier = Modifier.padding(vertical = 2.dp), color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.5f))
                distros.forEach { distro ->
                    val isInstalled = distro.id in installedDistros
                    val isActive = distro.id == activeDistroId
                    val canSelect = isInstalled
                    GlassSurface(
                        onClick = { if (canSelect) viewModel.setActiveDistro(distro.id) },
                        selected = isActive,
                        enabled = canSelect,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(horizontal = 16.dp, vertical = 14.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            RadioButton(
                                selected = isActive,
                                onClick = {},
                                enabled = canSelect
                            )
                            Spacer(Modifier.width(12.dp))
                            Column(modifier = Modifier.weight(1f)) {
                                Text(
                                    text = distro.name,
                                    style = MaterialTheme.typography.bodyLarge,
                                    fontWeight = if (isActive) FontWeight.SemiBold else FontWeight.Normal
                                )
                                Text(
                                    text = if (isInstalled) "Installed" else "Not installed",
                                    style = MaterialTheme.typography.bodySmall,
                                    color = if (isInstalled) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.error
                                )
                            }
                        }
                    }
                }
            }
        }

        GlassCard {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                SectionHeader(icon = Icons.Default.Security, title = "Execution Mode")
                HorizontalDivider(modifier = Modifier.padding(vertical = 2.dp), color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.5f))

                val currentMode by viewModel.executionMode.collectAsState()
                var showRestartDialog by remember { mutableStateOf(false) }
                var pendingMode by remember { mutableStateOf<ExecutionMode?>(null) }

                ExecutionMode.entries.forEach { mode ->
                    val isSelected = currentMode == mode
                    GlassSurface(
                        onClick = {
                            if (!isSelected) {
                                pendingMode = mode
                                showRestartDialog = true
                            }
                        },
                        selected = isSelected,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(horizontal = 16.dp, vertical = 14.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            RadioButton(selected = isSelected, onClick = {})
                            Spacer(Modifier.width(12.dp))
                            Column(modifier = Modifier.weight(1f)) {
                                Text(
                                    text = when (mode) {
                                        ExecutionMode.ROOT -> "Root (chroot)"
                                        ExecutionMode.PROOT -> "Rootless (proot)"
                                    },
                                    style = MaterialTheme.typography.bodyLarge,
                                    fontWeight = if (isSelected) FontWeight.SemiBold else FontWeight.Normal
                                )
                                Text(
                                    text = when (mode) {
                                        ExecutionMode.ROOT -> "Needs a rooted device. Real mounts, full system capabilities."
                                        ExecutionMode.PROOT -> "Works without root. User-space isolation via the bundled proot binary."
                                    },
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant
                                )
                            }
                        }
                    }
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    var seccompOff by remember {
                        mutableStateOf(!ProotManager.noSeccomp(appContext))
                    }
                    Column(modifier = Modifier.weight(1f)) {
                        Text(
                            text = "proot seccomp acceleration",
                            style = MaterialTheme.typography.bodyLarge
                        )
                        Text(
                            text = "Experimental: faster syscalls, may fail on some kernels. Restart the desktop to take effect.",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                    Switch(
                        checked = seccompOff,
                        onCheckedChange = {
                            seccompOff = it
                            ProotManager.setNoSeccomp(appContext, !it)
                        }
                    )
                }

                if (showRestartDialog && pendingMode != null) {
                    val leavingRoot = currentMode == ExecutionMode.ROOT && pendingMode == ExecutionMode.PROOT
                    AlertDialog(
                        onDismissRequest = { showRestartDialog = false; pendingMode = null },
                        title = { Text("Restart required") },
                        text = {
                            Text(
                                if (leavingRoot)
                                    "Switching from Root (chroot) to Rootless (proot): do NOT disable root until the switch and the first proot boot complete. Root is needed to delete the previous session's files (hidden/logs); without it, root-owned leftovers cannot be removed and the next boot may fail. You may disable root afterwards. The app will restart now."
                                else
                                    "Switching execution mode requires restarting the app. Previous session logs/hidden files will be cleaned automatically on next boot (with root when available). The new mode will take effect on next launch."
                            )
                        },
                        confirmButton = {
                            Button(onClick = {
                                val mode = pendingMode!!
                                viewModel.setExecutionMode(mode)
                                pendingMode = null
                                showRestartDialog = false
                                val ctx = appContext
                                val intent = ctx.packageManager.getLaunchIntentForPackage(ctx.packageName)
                                if (intent != null) {
                                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
                                    ctx.startActivity(intent)
                                }
                            }) { Text("Restart now") }
                        },
                        dismissButton = {
                            TextButton(onClick = { showRestartDialog = false; pendingMode = null }) { Text("Cancel") }
                        }
                    )
                }
            }
        }

        GlassCard {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionHeader(icon = Icons.Default.DisplaySettings, title = "Display")

                Spacer(Modifier.height(4.dp))

                listOf(resolution1080p, resolution720p, resolution540p).forEach { option ->
                    val isSelected = selectedResolution.label == option.label
                    GlassSurface(
                        onClick = {
                            selectedResolutionLabel = option.label
                            NativeBridge.setScaleSafe(option.scale)
                            onResolutionApplied("${option.label}: scale=${option.scale}")
                        },
                        selected = isSelected,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(horizontal = 16.dp, vertical = 14.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            RadioButton(
                                selected = isSelected,
                                onClick = {}
                            )
                            Spacer(Modifier.width(12.dp))
                            Text(
                                text = option.label,
                                style = MaterialTheme.typography.bodyLarge,
                                fontWeight = if (isSelected) FontWeight.SemiBold else FontWeight.Normal
                            )
                        }
                    }
                }
            }
        }

        GlassCard {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                SectionHeader(icon = Icons.Default.DisplaySettings, title = "Display Info")

                HorizontalDivider(modifier = Modifier.padding(vertical = 4.dp), color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.5f))

                Row(modifier = Modifier.fillMaxWidth()) {
                    Text("Window Bound", style = MaterialTheme.typography.bodyMedium, modifier = Modifier.weight(1f))
                    Text(
                        if (displayInfo.windowBound) "Yes" else if (displayInfo.logicalW == 0) "N/A" else "No",
                        style = MaterialTheme.typography.bodyMedium,
                        color = if (displayInfo.windowBound) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.error
                    )
                }
                Row(modifier = Modifier.fillMaxWidth()) {
                    Text("Logical (native)", style = MaterialTheme.typography.bodyMedium, modifier = Modifier.weight(1f))
                    Text(
                        if (displayInfo.logicalW == 0) "N/A" else "${displayInfo.logicalW} x ${displayInfo.logicalH}",
                        style = MaterialTheme.typography.bodyMedium
                    )
                }
                Row(modifier = Modifier.fillMaxWidth()) {
                    Text("Physical (viewport)", style = MaterialTheme.typography.bodyMedium, modifier = Modifier.weight(1f))
                    Text(
                        if (displayInfo.physicalW == 0) "N/A" else "${displayInfo.physicalW} x ${displayInfo.physicalH}",
                        style = MaterialTheme.typography.bodyMedium
                    )
                }
                Row(modifier = Modifier.fillMaxWidth()) {
                    Text("Scale", style = MaterialTheme.typography.bodyMedium, modifier = Modifier.weight(1f))
                    Text(
                        if (displayInfo.logicalW == 0) "N/A" else "${"%.2f".format(displayInfo.scaleW)} x ${"%.2f".format(displayInfo.scaleH)}",
                        style = MaterialTheme.typography.bodyMedium
                    )
                }
                Row(modifier = Modifier.fillMaxWidth()) {
                    Text("SHM", style = MaterialTheme.typography.bodyMedium, modifier = Modifier.weight(1f))
                    Text(
                        if (displayInfo.shmEnabled) "Enabled" else if (displayInfo.logicalW == 0) "N/A" else "Disabled",
                        style = MaterialTheme.typography.bodyMedium,
                        color = if (displayInfo.shmEnabled) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.error
                    )
                }
            }
        }

        GlassCard {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                SectionHeader(icon = Icons.Default.DarkMode, title = "Appearance")

                Text(
                    text = "Theme",
                    style = MaterialTheme.typography.labelLarge,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    data class ThemeOption(val label: String, val icon: androidx.compose.ui.graphics.vector.ImageVector, val onClick: () -> Unit, val isSelected: () -> Boolean)
                    val themeOptions = listOf(
                        ThemeOption("System", Icons.Default.PhoneAndroid, { localFollowSystem = true; onThemeModeChanged(true, localDarkMode) }, { localFollowSystem }),
                        ThemeOption("Dark", Icons.Default.DarkMode, { localFollowSystem = false; localDarkMode = true; onThemeModeChanged(false, true) }, { !localFollowSystem && localDarkMode }),
                        ThemeOption("Light", Icons.Default.LightMode, { localFollowSystem = false; localDarkMode = false; onThemeModeChanged(false, false) }, { !localFollowSystem && !localDarkMode }),
                    )
                    themeOptions.forEach { option ->
                        val isSelected = option.isSelected()
                        GlassSurface(
                            onClick = option.onClick,
                            selected = isSelected,
                            modifier = Modifier.weight(1f),
                            shape = RoundedCornerShape(12.dp)
                        ) {
                            Column(
                                modifier = Modifier.padding(vertical = 12.dp),
                                horizontalAlignment = Alignment.CenterHorizontally
                            ) {
                                Icon(
                                    imageVector = option.icon,
                                    contentDescription = option.label,
                                    tint = if (isSelected) MaterialTheme.colorScheme.onPrimaryContainer else MaterialTheme.colorScheme.onSurfaceVariant,
                                    modifier = Modifier.size(20.dp)
                                )
                                Spacer(Modifier.height(4.dp))
                                Text(
                                    text = option.label,
                                    style = MaterialTheme.typography.labelLarge,
                                    fontWeight = if (isSelected) FontWeight.SemiBold else FontWeight.Normal
                                )
                            }
                        }
                    }
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column(modifier = Modifier.weight(1f)) {
                        Text(
                            text = "Dynamic color",
                            style = MaterialTheme.typography.bodyLarge
                        )
                        Text(
                            text = "Follow wallpaper colors on Android 12+",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                    Switch(
                        checked = dynamicColorEnabled,
                        onCheckedChange = onDynamicColorChanged
                    )
                }

            }
        }

        GlassCard {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                SectionHeader(icon = Icons.Default.PowerSettingsNew, title = "Runtime Controls")
                val runtimeReady = activeDistroId != null
                val buttonsEnabled = controlsEnabled && runtimeReady

                Text(
                    text = if (runtimeReady) "Active distro: $activeDistroId" else "No active distro. Install and setup first.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )

                Row(horizontalArrangement = Arrangement.spacedBy(10.dp), modifier = Modifier.fillMaxWidth()) {
                    Button(
                        onClick = { confirmAction = ConfirmAction.STOP },
                        enabled = buttonsEnabled,
                        modifier = Modifier.weight(1f).heightIn(min = 52.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = StopRed, contentColor = Color.White)
                    ) {
                        Text("Stop")
                    }
                    Button(
                        onClick = { confirmAction = ConfirmAction.RESTART },
                        enabled = buttonsEnabled,
                        modifier = Modifier.weight(1f).heightIn(min = 52.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = RestartBlue, contentColor = Color.White)
                    ) {
                        Text("Restart")
                    }
                }
                OutlinedButton(
                    onClick = onRequestUsb,
                    enabled = controlsEnabled,
                    modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)
                ) {
                    Icon(Icons.Default.Usb, contentDescription = "Request USB")
                    Spacer(Modifier.width(8.dp))
                    Text("USB")
                }
            }
        }
        GlassCard {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionHeader(icon = Icons.Default.TouchApp, title = "Input Mode")

                Spacer(Modifier.height(4.dp))

                val currentMode by viewModel.inputMode.collectAsState()

                MainViewModel.InputMode.entries.forEach { mode ->
                    val isSelected = currentMode == mode
                    GlassSurface(
                        onClick = { viewModel.updateInputMode(mode) },
                        selected = isSelected,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(horizontal = 16.dp, vertical = 14.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            RadioButton(
                                selected = isSelected,
                                onClick = {}
                            )
                            Spacer(Modifier.width(12.dp))
                            Text(
                                text = "${mode.name} Mode",
                                style = MaterialTheme.typography.bodyLarge,
                                fontWeight = if (isSelected) FontWeight.SemiBold else FontWeight.Medium
                            )
                            Spacer(Modifier.weight(1f))
                            Icon(
                                imageVector = when (mode) {
                                    MainViewModel.InputMode.Touch -> Icons.Default.TouchApp
                                    MainViewModel.InputMode.Trackpad -> Icons.Default.Laptop
                                    MainViewModel.InputMode.Mouse -> Icons.Default.Mouse
                                },
                                contentDescription = "${mode.name} mode",
                                tint = if (isSelected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant,
                                modifier = Modifier.size(20.dp)
                            )
                        }
                    }
                }
            }
        }

        GlassCard {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                SectionHeader(icon = Icons.Default.Info, title = "Help")
                HorizontalDivider(modifier = Modifier.padding(vertical = 2.dp), color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.5f))

                val features = listOf(
                    "Linux Desktop" to "Run full XFCE desktop environment with Wayland compositor on your Android device.",
                    "Multiple Distros" to "Support for Ubuntu, Debian, and other Linux distributions with easy install and setup.",
                    "Terminal Emulator" to "Built-in terminal with proot/chroot sessions, extra keys, and session management.",
                    "Root & Rootless" to "Choose between root mode (chroot) for full capabilities or rootless mode (proot) without root access.",
                    "GPU Support" to "Hardware-accelerated rendering via Vulkan/OpenGL ES when running in root mode.",
                    "Display Scaling" to "Adjust display resolution and scale factor for optimal viewing on any screen size.",
                    "USB Devices" to "Pass-through USB devices like keyboard and mouse to the Linux environment.",
                    "Clipboard Sync" to "Seamless clipboard sharing between Android and the Linux desktop.",
                    "Wayland Compositor" to "Native Wayland display server using Smithay for high-performance window management.",
                    "Proot Bundled" to "Built-in proot binary compiled for aarch64. No external downloads needed for rootless mode."
                )

                features.forEach { (title, description) ->
                    Column(modifier = Modifier.fillMaxWidth()) {
                        Text(
                            text = title,
                            style = MaterialTheme.typography.bodyLarge,
                            fontWeight = FontWeight.SemiBold,
                            color = MaterialTheme.colorScheme.primary
                        )
                        Text(
                            text = description,
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                }

                HorizontalDivider(modifier = Modifier.padding(vertical = 2.dp), color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.5f))

                Text(
                    text = "Winland Server v1.0",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.fillMaxWidth(),
                    textAlign = androidx.compose.ui.text.style.TextAlign.Center
                )
            }
        }

        when (confirmAction) {
            ConfirmAction.STOP -> AlertDialog(
                onDismissRequest = { confirmAction = null },
                title = { Text("Stop distro?") },
                text = { Text("This will stop the running Linux desktop environment.") },
                confirmButton = {
                    Button(
                        onClick = { confirmAction = null; onStopChroot() },
                        colors = ButtonDefaults.buttonColors(containerColor = StopRed, contentColor = Color.White)
                    ) { Text("Stop") }
                },
                dismissButton = {
                    TextButton(onClick = { confirmAction = null }) { Text("Cancel") }
                }
            )
            ConfirmAction.RESTART -> AlertDialog(
                onDismissRequest = { confirmAction = null },
                title = { Text("Restart distro?") },
                text = { Text("This will restart the Linux desktop environment.") },
                confirmButton = {
                    Button(
                        onClick = { confirmAction = null; onRestartChroot() },
                        colors = ButtonDefaults.buttonColors(containerColor = RestartBlue, contentColor = Color.White)
                    ) { Text("Restart") }
                },
                dismissButton = {
                    TextButton(onClick = { confirmAction = null }) { Text("Cancel") }
                }
            )
            null -> {}
        }

        Spacer(Modifier.height(20.dp))
    }
}

@Composable
private fun DistroCard(
    distro: LinuxDistro,
    viewModel: MainViewModel,
    actions: WinlandDashboardActions
) {
    val haptics = LocalHapticFeedback.current
    val activeUiOperation by viewModel.activeUiOperation.collectAsState()
    val isSettingUp = activeUiOperation == MainViewModel.UiOperation.SETUP
    val distroUiStates by viewModel.distroUiStates.collectAsState()
    val currentUiState = distroUiStates[distro.id] ?: MainViewModel.DistroUiState()
    val progress = currentUiState.progress
    val isDownloading = currentUiState.isDownloading
    val isRunLaunching = currentUiState.isRunLaunching
    val stageText = currentUiState.stageText
    val lastStageUpdate = currentUiState.lastStageUpdate
    val updateStage: (String) -> Unit = { next -> viewModel.updateDistroStage(distro.id, next) }
    val perDistroStates by viewModel.perDistroChrootState.collectAsState()
    val refreshSignal by viewModel.perDistroRefreshSignal.collectAsState()
    val distroChrootState = perDistroStates[distro.id]

    LaunchedEffect(distro.id, refreshSignal) {
        viewModel.refreshChrootRuntimeStateForDistro(distro.id)
    }

    val currentStage = when {
        distroChrootState?.ready == true -> "run"
        distroChrootState?.isExtracted == true -> "setup"
        else -> "install"
    }
    val statusReady = distroChrootState?.ready == true
    val statusText = distroChrootState?.reason ?: "Checking status..."
    val executionMode by viewModel.executionMode.collectAsState()

    val operationLocked = activeUiOperation != null
    val lockReason = when {
        activeUiOperation == MainViewModel.UiOperation.DOWNLOAD -> "Download in progress"
        activeUiOperation == MainViewModel.UiOperation.SETUP -> "Setup in progress"
        activeUiOperation == MainViewModel.UiOperation.RUN -> "Run in progress"
        else -> null
    }

    LaunchedEffect(currentStage, isDownloading, isSettingUp, isRunLaunching) {
        val nextStage = when {
            isDownloading -> "Downloading and extracting rootfs"
            isSettingUp -> "Setup in progress (installing packages)"
            isRunLaunching -> "Run launching"
            currentStage == "setup" -> "Ready for setup"
            currentStage == "run" -> "Ready to run"
            else -> "Waiting for archive download"
        }
        if (stageText != nextStage) {
            updateStage(nextStage)
        }
    }

    LaunchedEffect(isSettingUp) {
        if (!isSettingUp) return@LaunchedEffect
        ChrootInstaller.logFlow.collect {
            viewModel.touchDistroUpdate(distro.id)
        }
    }

    GlassCard(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(24.dp)
    ) {
        Column(modifier = Modifier.padding(20.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(distro.name, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
                    Text(distro.description, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                Column(horizontalAlignment = Alignment.End) {
                    val tone = when {
                        isDownloading || isSettingUp || isRunLaunching -> BadgeTone.Info
                        statusReady -> BadgeTone.Success
                        else -> BadgeTone.Neutral
                    }
                    val label = when {
                        isDownloading -> "DOWNLOADING"
                        isSettingUp -> "SETTING UP"
                        isRunLaunching -> "LAUNCHING"
                        statusReady -> "READY"
                        else -> "PENDING"
                    }
                    StatusBadge(text = label, tone = tone)
                    Spacer(Modifier.height(4.dp))
                    ExecutionModeBadge(mode = executionMode)
                }
            }

            Spacer(Modifier.height(8.dp))
            Text(
                "Status: $statusText",
                style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            Text(
                "Phase: $stageText  |  Last update: $lastStageUpdate",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )

            if (isDownloading) {
                LinearProgressIndicator(
                    progress = { progress },
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 8.dp)
                        .height(6.dp)
                        .clip(CircleShape),
                    color = MaterialTheme.colorScheme.primary,
                    trackColor = MaterialTheme.colorScheme.primary.copy(alpha = 0.1f)
                )
            } else if (isSettingUp) {
                LinearProgressIndicator(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 8.dp)
                        .height(6.dp)
                        .clip(CircleShape)
                )
            }

            if (isDownloading) {
                Text(
                    text = formatDownloadDetail(
                        fraction = progress,
                        bytesRead = currentUiState.downloadedBytes,
                        totalBytes = currentUiState.totalBytes,
                        bytesPerSec = currentUiState.speedBps
                    ),
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    fontFamily = FontFamily.Monospace
                )
                Spacer(Modifier.height(4.dp))
            }

            if (operationLocked) {
                Text(
                    text = "Controls locked: ${lockReason ?: "Operation running"}",
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.secondary
                )
            }

            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                if (currentStage != "run") {
                    Button(
                        onClick = {
                            haptics.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                            when (currentStage) {
                                "install" -> {
                                    if (operationLocked) {
                                        actions.onShowMessage("Another operation is running. Wait until it finishes.", false)
                                        return@Button
                                    }
                                    actions.onDistroInstall(distro)
                                }

                                "setup" -> {
                                    if (operationLocked || isSettingUp) {
                                        actions.onShowMessage("Another operation is running. Wait until it finishes.", false)
                                        return@Button
                                    }
                                    actions.onDistroSetup(distro.id)
                                }

                                else -> Unit
                            }
                        },
                        enabled = !operationLocked && !isDownloading && !isSettingUp && !isRunLaunching
                    ) {
                        Icon(Icons.Default.Download, "Download/Install")
                        Spacer(Modifier.width(6.dp))
                        Text(
                            when {
                                isDownloading -> "Downloading..."
                                operationLocked && currentStage == "install" -> "Install Locked"
                                operationLocked && currentStage == "setup" -> "Setup Locked"
                                isRunLaunching -> "Launching..."
                                isSettingUp -> "Setup..."
                                currentStage == "setup" -> "Setup"
                                else -> "Install"
                            }
                        )
                    }
                } else {
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Button(
                            onClick = actions.onDistroStop,
                            colors = ButtonDefaults.buttonColors(containerColor = StopRed, contentColor = Color.White)
                        ) {
                            Text("Stop")
                        }

                        Button(
                            onClick = actions.onDistroRestart,
                            colors = ButtonDefaults.buttonColors(containerColor = RestartBlue, contentColor = Color.White)
                        ) {
                            Text("Restart")
                        }

                        Button(
                            onClick = {
                                haptics.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                                if (operationLocked) {
                                    actions.onShowMessage("Download/Setup is running. Wait before Run.", false)
                                    return@Button
                                }
                                if (isRunLaunching) {
                                    actions.onShowMessage("Run is already in progress", false)
                                    return@Button
                                }
                                actions.onDistroRun(distro.id)
                            },
                            enabled = !operationLocked && !isRunLaunching && !isDownloading && !isSettingUp,
                            colors = ButtonDefaults.buttonColors(containerColor = RunGreen, contentColor = Color.White)
                        ) {
                            Icon(Icons.Default.PlayArrow, "Run desktop")
                            Spacer(Modifier.width(6.dp))
                            Text(if (isRunLaunching) "Launching..." else "Run")
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun ExecutionModeBadge(mode: ExecutionMode) {
    val isProot = mode == ExecutionMode.PROOT
    Surface(
        color = if (isProot) MaterialTheme.colorScheme.tertiaryContainer else MaterialTheme.colorScheme.secondaryContainer,
        shape = RoundedCornerShape(50)
    ) {
        Text(
            text = if (isProot) "ROOTLESS" else "ROOT",
            style = MaterialTheme.typography.labelSmall,
            color = if (isProot) MaterialTheme.colorScheme.onTertiaryContainer else MaterialTheme.colorScheme.onSecondaryContainer,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.padding(horizontal = 10.dp, vertical = 5.dp)
        )
    }
}

/* Shared StatusBadge(text, tone) from WinlandComponents.kt is used instead. */

private fun formatBytes(bytes: Long): String = when {
    bytes < 0 -> "?"
    bytes < 1024L -> "$bytes B"
    bytes < 1024L * 1024L -> "%.1f KB".format(bytes / 1024f)
    bytes < 1024L * 1024L * 1024L -> "%.1f MB".format(bytes / 1048576f)
    else -> "%.2f GB".format(bytes / 1073741824f)
}

private fun formatEta(remainingBytes: Long, bytesPerSec: Long): String {
    if (bytesPerSec <= 0L || remainingBytes <= 0L) return "--:--"
    val totalSec = remainingBytes / bytesPerSec
    return "%02d:%02d".format(totalSec / 60, totalSec % 60)
}

/** "42% • 12.6/30.0 MB • 1.2 MB/s • ETA 00:14" (parts omitted when unknown). */
private fun formatDownloadDetail(fraction: Float, bytesRead: Long, totalBytes: Long, bytesPerSec: Long): String {
    val parts = mutableListOf<String>()
    if (fraction >= 0f) parts.add("${(fraction * 100).toInt()}%")
    if (totalBytes > 0) {
        parts.add("${formatBytes(bytesRead)}/${formatBytes(totalBytes)}")
        parts.add(formatEta(totalBytes - bytesRead, bytesPerSec))
            .let { eta -> "ETA $eta" }
    } else if (bytesRead > 0) {
        parts.add(formatBytes(bytesRead))
    }
    if (bytesPerSec > 0) parts.add("${formatBytes(bytesPerSec)}/s")
    return parts.joinToString(" • ").ifEmpty { "Downloading..." }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ProfessionalTopBar(activeOperationText: String?) {
    CenterAlignedTopAppBar(
        title = {
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Text("Winland", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
                if (activeOperationText != null) {
                    Text(
                        activeOperationText,
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.primary
                    )
                } else {
                    Text(
                        "Linux Desktop Environment",
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
        },
        colors = TopAppBarDefaults.centerAlignedTopAppBarColors(
            containerColor = MaterialTheme.colorScheme.surface.copy(alpha = 0.92f)
        )
    )
}

@Composable
private fun ModernNavigationBar(selectedTab: DashboardTab, onTabSelected: (DashboardTab) -> Unit, modifier: Modifier = Modifier) {
    Surface(
        modifier = modifier
            .padding(horizontal = 24.dp, vertical = 12.dp)
            .height(72.dp)
            .fillMaxWidth(),
        color = MaterialTheme.colorScheme.surface.copy(alpha = 0.95f),
        shape = CircleShape,
        shadowElevation = 12.dp
    ) {
        Row(
            modifier = Modifier.fillMaxSize(),
            horizontalArrangement = Arrangement.SpaceEvenly,
            verticalAlignment = Alignment.CenterVertically
        ) {
            val tabs = listOf(
                Triple(DashboardTab.Home, Icons.Default.Home, "Home"),
                Triple(DashboardTab.Terminal, Icons.Default.Terminal, "Terminal"),
                Triple(DashboardTab.Settings, Icons.Default.Settings, "Settings")
            )
            tabs.forEach { (tab, icon, label) ->
                val isSelected = selectedTab == tab
                val color by animateColorAsState(
                    targetValue = if (isSelected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant,
                    label = "tabColor"
                )
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    modifier = Modifier
                        .clip(RoundedCornerShape(16.dp))
                        .then(
                            Modifier
                                .background(
                                    if (isSelected) MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.3f)
                                    else MaterialTheme.colorScheme.surface.copy(alpha = 0f)
                                )
                                .padding(horizontal = 16.dp, vertical = 6.dp)
                        )
                ) {
                    IconButton(
                        onClick = { onTabSelected(tab) },
                        modifier = Modifier.size(28.dp)
                    ) {
                        Icon(
                            imageVector = icon,
                            contentDescription = label,
                            tint = color,
                            modifier = Modifier.size(if (isSelected) 24.dp else 22.dp)
                        )
                    }
                    Text(
                        text = label,
                        style = MaterialTheme.typography.labelSmall,
                        color = color,
                        fontWeight = if (isSelected) FontWeight.SemiBold else FontWeight.Normal,
                        fontSize = 10.sp
                    )
                }
            }
        }
    }
}

@Composable
private fun SessionSwitcherBar(
    sessionTabs: List<TerminalSessionTab>,
    activeSessionId: String,
    onSelectTab: (String) -> Unit,
    onCloseTab: (String) -> Unit,
    onAddTab: () -> Unit,
    onRenameRequest: (TerminalSessionTab) -> Unit,
    modifier: Modifier = Modifier
) {
    val activeLabel = sessionTabs.first { it.id == activeSessionId }.label.value
    var expanded by remember { mutableStateOf(false) }

    Row(
        modifier = modifier
            .fillMaxWidth()
            .background(MaterialTheme.colorScheme.surfaceContainerHigh)
            .padding(horizontal = 12.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Box {
            Row(
                modifier = Modifier
                    .clickable { expanded = true }
                    .padding(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(activeLabel, color = MaterialTheme.colorScheme.onSurface, fontWeight = FontWeight.SemiBold)
                Spacer(Modifier.width(4.dp))
                Icon(
                    Icons.Default.KeyboardArrowDown,
                    contentDescription = "Switch session",
                    tint = MaterialTheme.colorScheme.onSurface
                )
            }

            DropdownMenu(
                expanded = expanded,
                onDismissRequest = { expanded = false }
            ) {
                sessionTabs.forEach { tab ->
                    val isActive = tab.id == activeSessionId
                    DropdownMenuItem(
                        onClick = {
                            onSelectTab(tab.id)
                            expanded = false
                        },
                        text = {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Text(
                                    if (isActive) "\u25CF" else "\u25CB",
                                    color = if (isActive) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant,
                                    fontSize = 12.sp
                                )
                                Spacer(Modifier.width(8.dp))
                                Text(
                                    tab.label.value,
                                    color = MaterialTheme.colorScheme.onSurface,
                                    fontSize = 13.sp
                                )
                            }
                        },
                        trailingIcon = {
                            if (sessionTabs.size > 1) {
                                IconButton(
                                    onClick = { onCloseTab(tab.id) },
                                    modifier = Modifier.size(24.dp)
                                ) {
                                    Icon(
                                        Icons.Default.Close,
                                        contentDescription = "Close session",
                                        tint = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f),
                                        modifier = Modifier.size(16.dp)
                                    )
                                }
                            }
                        }
                    )
                }
                HorizontalDivider()
                DropdownMenuItem(
                    onClick = { onAddTab(); expanded = false },
                    text = {
                        Text("\uFF0B New Session", color = MaterialTheme.colorScheme.primary)
                    }
                )
            }
        }
        IconButton(onClick = {
            onRenameRequest(sessionTabs.first { it.id == activeSessionId })
        }) {
            Icon(
                Icons.Default.Edit,
                contentDescription = "Rename session",
                tint = MaterialTheme.colorScheme.onSurface
            )
        }
    }
}

@Composable
private fun TerminalExtraKeysBar(
    ctrlActive: Boolean,
    altActive: Boolean,
    onCtrlToggle: () -> Unit,
    onAltToggle: () -> Unit,
    onKey: (String) -> Unit,
    modifier: Modifier = Modifier
) {
    val scrollState = rememberScrollState()
    Row(
        modifier = modifier
            .fillMaxWidth()
            .background(MaterialTheme.colorScheme.surfaceContainerHigh)
            .horizontalScroll(scrollState)
            .padding(horizontal = 6.dp, vertical = 5.dp),
        horizontalArrangement = Arrangement.spacedBy(4.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        ExtraKeyButton("ESC", onKey)
        ExtraKeyToggle("CTL", ctrlActive, onCtrlToggle)
        ExtraKeyToggle("ALT", altActive, onAltToggle)
        ExtraKeyButton("TAB", onKey)
        ExtraKeyButton("/", onKey)
        ExtraKeyButton("-", onKey)
        ExtraKeyButton("|", onKey)
        ExtraKeyButton("~", onKey)
        ExtraKeyButton("◀", onClick = { onKey("LEFT") }, repeatable = true)
        ExtraKeyButton("▲", onClick = { onKey("UP") }, repeatable = true)
        ExtraKeyButton("▼", onClick = { onKey("DOWN") }, repeatable = true)
        ExtraKeyButton("▶", onClick = { onKey("RIGHT") }, repeatable = true)
        ExtraKeyButton("HM", onClick = { onKey("HOME") })
        ExtraKeyButton("EN", onClick = { onKey("END") })
        ExtraKeyButton("PU", onClick = { onKey("PGUP") })
        ExtraKeyButton("PD", onClick = { onKey("PGDN") })
        ExtraKeyButton("DEL", onKey)
    }
}

@Composable
private fun ExtraKeyButton(
    label: String,
    onKey: (String) -> Unit = {},
    onClick: (() -> Unit)? = null,
    repeatable: Boolean = false
) {
    val haptics = LocalHapticFeedback.current
    // Quiet fire shared by tap and repeat paths.
    val fireQuiet = { onClick?.invoke() ?: onKey(label) }
    val fire = {
        haptics.performHapticFeedback(HapticFeedbackType.TextHandleMove)
        fireQuiet()
    }
    // Repeat machinery (arrows only). The press stream is ALWAYS drained so
    // emissions never stall the button; repeat logic runs only when repeatable.
    val interactionSource = remember { MutableInteractionSource() }
    val scope = rememberCoroutineScope()
    var repeatJob: Job? = remember { null }
    var suppressClick by remember { mutableStateOf(false) }
    LaunchedEffect(Unit) {
        interactionSource.interactions.collect { interaction ->
            if (!repeatable) return@collect
            when (interaction) {
                is PressInteraction.Press -> {
                    repeatJob?.cancel()
                    suppressClick = false
                    repeatJob = scope.launch {
                        delay(400)
                        suppressClick = true
                        while (true) {
                            fireQuiet()
                            delay(50)
                        }
                    }
                }
                is PressInteraction.Release -> {
                    repeatJob?.cancel()
                    repeatJob = null
                }
                is PressInteraction.Cancel -> {
                    repeatJob?.cancel()
                    repeatJob = null
                    suppressClick = false
                }
            }
        }
    }
    Surface(
        onClick = {
            if (!repeatable) {
                fire()
            } else {
                // Release after hold: repeats already fired, skip the extra.
                // Quick tap: nothing fired yet, fire exactly once (as before).
                repeatJob?.cancel()
                repeatJob = null
                if (suppressClick) suppressClick = false else fire()
            }
        },
        interactionSource = interactionSource,
        shape = RoundedCornerShape(8.dp),
        color = MaterialTheme.colorScheme.surfaceVariant,
        modifier = Modifier.height(40.dp)
    ) {
        Box(
            contentAlignment = Alignment.Center,
            modifier = Modifier.padding(horizontal = 12.dp)
        ) {
            Text(
                text = label,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                fontSize = 13.sp,
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.SemiBold
            )
        }
    }
}

@Composable
private fun ExtraKeyToggle(
    label: String,
    active: Boolean,
    onClick: () -> Unit
) {
    val haptics = LocalHapticFeedback.current
    val bgColor by animateColorAsState(
        targetValue = if (active) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant,
        label = "toggleBg"
    )
    val textColor = if (active) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurfaceVariant
    Surface(
        onClick = {
            haptics.performHapticFeedback(HapticFeedbackType.TextHandleMove)
            onClick()
        },
        shape = RoundedCornerShape(8.dp),
        color = bgColor,
        modifier = Modifier.height(40.dp)
    ) {
        Box(
            contentAlignment = Alignment.Center,
            modifier = Modifier.padding(horizontal = 12.dp)
        ) {
            Text(
                text = label,
                color = textColor,
                fontSize = 13.sp,
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.Bold
            )
        }
    }
}
