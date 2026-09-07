package com.winland.server.ui.theme

import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.ui.graphics.Color

// Winland professional-blue identity. Replaces the stock Material3 purple
// baseline. Both schemes are blue-tinted surfaces with a strong blue primary.

val WinlandDarkColorScheme = darkColorScheme(
    primary = Color(0xFFAAC7FF),
    onPrimary = Color(0xFF0A2F60),
    primaryContainer = Color(0xFF2F5DA8),
    onPrimaryContainer = Color(0xFFD8E2FF),
    secondary = Color(0xFFBBC7DB),
    onSecondary = Color(0xFF253141),
    secondaryContainer = Color(0xFF3B4758),
    onSecondaryContainer = Color(0xFFD8E2F8),
    tertiary = Color(0xFF7BD79A),
    onTertiary = Color(0xFF0C381E),
    tertiaryContainer = Color(0xFF1E4D2C),
    onTertiaryContainer = Color(0xFFBDF0C4),
    error = Color(0xFFFFB4AB),
    onError = Color(0xFF690005),
    errorContainer = Color(0xFF93000A),
    onErrorContainer = Color(0xFFFFDAD6),
    background = Color(0xFF111318),
    onBackground = Color(0xFFE2E2E9),
    surface = Color(0xFF111318),
    onSurface = Color(0xFFE2E2E9),
    surfaceVariant = Color(0xFF44474E),
    onSurfaceVariant = Color(0xFFC3C6CF),
    surfaceContainerHigh = Color(0xFF1D2026),
    outline = Color(0xFF8E9099),
    outlineVariant = Color(0xFF44474E),
    inverseSurface = Color(0xFFE2E2E9),
    inverseOnSurface = Color(0xFF111318),
    inversePrimary = Color(0xFF2F5DA8),
    surfaceTint = Color(0xFFAAC7FF),
)

val WinlandLightColorScheme = lightColorScheme(
    primary = Color(0xFF2F5DA8),
    onPrimary = Color(0xFFFFFFFF),
    primaryContainer = Color(0xFFD8E2FF),
    onPrimaryContainer = Color(0xFF001A40),
    secondary = Color(0xFF545F70),
    onSecondary = Color(0xFFFFFFFF),
    secondaryContainer = Color(0xFFD8E2F8),
    onSecondaryContainer = Color(0xFF111C2B),
    tertiary = Color(0xFF3B6B4F),
    onTertiary = Color(0xFFFFFFFF),
    tertiaryContainer = Color(0xFFBDF0C4),
    onTertiaryContainer = Color(0xFF072711),
    error = Color(0xFFBA1A1A),
    onError = Color(0xFFFFFFFF),
    errorContainer = Color(0xFFFFDAD6),
    onErrorContainer = Color(0xFF410002),
    background = Color(0xFFF9F9FF),
    onBackground = Color(0xFF191C22),
    surface = Color(0xFFF9F9FF),
    onSurface = Color(0xFF191C22),
    surfaceVariant = Color(0xFFE0E2EC),
    onSurfaceVariant = Color(0xFF44474E),
    surfaceContainerHigh = Color(0xFFECEEF4),
    outline = Color(0xFF74777F),
    outlineVariant = Color(0xFFC3C6CF),
    inverseSurface = Color(0xFF2E3036),
    inverseOnSurface = Color(0xFFF0F0F7),
    inversePrimary = Color(0xFFAAC7FF),
    surfaceTint = Color(0xFF2F5DA8),
)

// Semantic action colors (Run/Restart/Stop). Single hues readable on both
// themes with white content; used until they become full theme roles.
val ActionGreen = Color(0xFF43A047)
val ActionBlue = Color(0xFF1E88E5)
val ActionRed = Color(0xFFE53935)
val OnAction = Color(0xFFFFFFFF)
