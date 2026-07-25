package com.winland.server.ui

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ElevatedCard
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

val LocalGlassMode = compositionLocalOf { false }

private val glassAlphaDark: Float = 0.60f
private val glassAlphaLight: Float = 0.50f
private val glassBorderAlphaDark: Float = 0.15f
private val glassBorderAlphaLight: Float = 0.25f

@Composable
private fun glassConfig(): GlassConfig {
    val bg = MaterialTheme.colorScheme.background
    val isDark = (bg.red * 0.299f + bg.green * 0.587f + bg.blue * 0.114f) < 0.5f
    return GlassConfig(
        backgroundAlpha = if (isDark) glassAlphaDark else glassAlphaLight,
        borderAlpha = if (isDark) glassBorderAlphaDark else glassBorderAlphaLight
    )
}

private data class GlassConfig(
    val backgroundAlpha: Float,
    val borderAlpha: Float
)

@Composable
fun GlassCard(
    modifier: Modifier = Modifier,
    shape: RoundedCornerShape = RoundedCornerShape(24.dp),
    content: @Composable ColumnScope.() -> Unit
) {
    if (!LocalGlassMode.current) {
        ElevatedCard(
            modifier = modifier,
            shape = shape,
            colors = CardDefaults.elevatedCardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerHigh)
        ) {
            Column(content = content)
        }
        return
    }
    val cfg = glassConfig()
    ElevatedCard(
        modifier = modifier.border(0.5.dp, MaterialTheme.colorScheme.outlineVariant.copy(alpha = cfg.borderAlpha), shape),
        shape = shape,
        colors = CardDefaults.elevatedCardColors(
            containerColor = MaterialTheme.colorScheme.surface.copy(alpha = cfg.backgroundAlpha)
        ),
        elevation = CardDefaults.elevatedCardElevation(defaultElevation = 4.dp)
    ) {
        Column(content = content)
    }
}

@Composable
fun GlassSurface(
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    selected: Boolean = false,
    shape: RoundedCornerShape = RoundedCornerShape(12.dp),
    enabled: Boolean = true,
    content: @Composable () -> Unit
) {
    if (!LocalGlassMode.current) {
        Surface(
            onClick = onClick,
            modifier = modifier,
            shape = shape,
            color = if (selected) MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.3f) else MaterialTheme.colorScheme.surface,
            tonalElevation = if (selected) 2.dp else 0.dp,
            enabled = enabled
        ) {
            content()
        }
        return
    }
    val cfg = glassConfig()
    val bg = if (selected) {
        MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.4f)
    } else {
        MaterialTheme.colorScheme.surface.copy(alpha = cfg.backgroundAlpha * 0.9f)
    }
    val borderColor = if (selected) {
        MaterialTheme.colorScheme.primary.copy(alpha = 0.3f)
    } else {
        MaterialTheme.colorScheme.outlineVariant.copy(alpha = cfg.borderAlpha)
    }
    Surface(
        onClick = onClick,
        modifier = modifier,
        shape = shape,
        color = bg,
        tonalElevation = if (selected) 2.dp else 0.dp,
        border = BorderStroke(if (selected) 1.dp else 0.5.dp, borderColor),
        enabled = enabled
    ) {
        content()
    }
}
