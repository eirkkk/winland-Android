package com.winland.server

data class LinuxDistro(
    val id: String,
    val name: String,
    val description: String,
    val url: String
)

enum class DashboardTab {
    Home,
    Terminal,
    Settings
}