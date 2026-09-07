package com.winland.server.ui.theme

import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Shapes
import androidx.compose.ui.unit.dp

// Unified corner radii. Cards = medium (14dp), buttons/inputs = small (12dp),
// badges and nav pills = full circle.
val WinlandShapes = Shapes(
    extraSmall = RoundedCornerShape(4.dp),
    small = RoundedCornerShape(8.dp),
    medium = RoundedCornerShape(14.dp),
    large = RoundedCornerShape(20.dp),
    extraLarge = RoundedCornerShape(28.dp),
)

/** Standard card shape used by WinlandCard/GlassCard. */
val WinlandCardShape = RoundedCornerShape(14.dp)

/** Standard shape for action buttons and selectable rows. */
val WinlandControlShape = RoundedCornerShape(12.dp)
