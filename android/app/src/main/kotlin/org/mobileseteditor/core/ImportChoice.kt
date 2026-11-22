// Mobile Set Editor - which Templates of a scanned zip to install: dependencies follow (ADR 0005, #71)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor.core

/**
 * The user's choice of what to install from a scanned zip ([packages]: names; [fonts]: the loose
 * fonts and dictionaries). As MSE's installer: picking a package picks the packages in the zip it
 * depends on that still need installing; clearing one that a picked package needs clears that
 * package too. Only installable packages (new, or newer than the installed one) can be picked.
 */
data class ImportChoice(val scan: ScanResult, val packages: Set<String>, val fonts: Boolean) {
    private val byName get() = scan.packages.associateBy { it.name }

    /** The installable packages in the zip that [name] needs, directly or not */
    fun dependenciesOf(name: String): Set<String> {
        val all = byName
        val out = mutableSetOf<String>()
        val todo = ArrayDeque(all[name]?.dependencies?.toList().orEmpty())
        while (todo.isNotEmpty()) {
            val dep = all[todo.removeFirst()] ?: continue
            if (!dep.installable || !out.add(dep.name)) continue
            todo.addAll(dep.dependencies)
        }
        return out
    }

    /** The picked packages that need [name] (so it is picked because of them) */
    fun neededBy(name: String): List<String> = packages.filter { it != name && name in dependenciesOf(it) }

    fun select(name: String): ImportChoice {
        if (byName[name]?.installable != true) return this
        return copy(packages = packages + name + dependenciesOf(name))
    }

    fun clear(name: String): ImportChoice = copy(packages = packages - name - neededBy(name).toSet())

    fun toggle(name: String) = if (name in packages) clear(name) else select(name)

    /** Pick or clear every installable package of an installer group, and of the groups under it ("magic/M15 Frames" has "magic/M15 Frames/Standard") */
    fun selectGroup(group: String) = scan.packages.filter { inGroup(it.installerGroup, group) }.fold(this) { c, p -> c.select(p.name) }
    fun clearGroup(group: String) = scan.packages.filter { inGroup(it.installerGroup, group) }.fold(this) { c, p -> c.clear(p.name) }

    /** Bytes the choice would extract */
    val size: Long get() = byName.let { all -> packages.sumOf { all[it]?.size ?: 0 } } + if (fonts) scan.newFontsSize else 0

    val isEmpty: Boolean get() = packages.isEmpty() && !(fonts && (scan.newFontCount > 0 || scan.newDictionaryCount > 0))

    companion object {
        /** Is a package of installer group [group] in [under], or in a group under it? */
        fun inGroup(group: String, under: String) = group == under || group.startsWith("$under/")

        /** Everything that can be installed */
        fun everything(scan: ScanResult) = ImportChoice(scan, scan.packages.filter { it.installable }.map { it.name }.toSet(), true)
        fun nothing(scan: ScanResult) = ImportChoice(scan, emptySet(), false)
    }
}
