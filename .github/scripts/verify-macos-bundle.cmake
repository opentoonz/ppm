# CI-only check: every non-system dependency, including plugins, must be bundled.
if(NOT IS_DIRECTORY "${BUNDLE}")
    message(FATAL_ERROR "BUNDLE must name the staged application")
endif()
set(executable "${BUNDLE}/Contents/MacOS/PPM")
file(GLOB_RECURSE plugins "${BUNDLE}/Contents/PlugIns/*.dylib")
if(NOT EXISTS "${BUNDLE}/Contents/PlugIns/platforms/libqcocoa.dylib" OR
   NOT EXISTS "${BUNDLE}/Contents/PlugIns/iconengines/libqsvgicon.dylib")
    message(FATAL_ERROR "The Cocoa and SVG icon plugins must be bundled")
endif()
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${executable}"
    BUNDLE_EXECUTABLE "${executable}"
    MODULES ${plugins}
    RESOLVED_DEPENDENCIES_VAR dependencies
    UNRESOLVED_DEPENDENCIES_VAR missing
    CONFLICTING_DEPENDENCIES_PREFIX conflicts
    PRE_EXCLUDE_REGEXES "^/System/Library/" "^/usr/lib/")
if(missing)
    message(FATAL_ERROR "Unresolved dependencies: ${missing}")
endif()
foreach(name IN LISTS conflicts_FILENAMES)
    set(paths "")
    foreach(path IN LISTS conflicts_${name})
        file(REAL_PATH "${path}" path)
        list(APPEND paths "${path}")
    endforeach()
    list(REMOVE_DUPLICATES paths)
    list(LENGTH paths count)
    if(count GREATER 1)
        message(FATAL_ERROR "Conflicting dependency ${name}: ${paths}")
    endif()
    list(APPEND dependencies ${paths})
endforeach()
file(REAL_PATH "${BUNDLE}" bundle_real)
foreach(dependency IN LISTS dependencies)
    file(REAL_PATH "${dependency}" dependency)
    string(FIND "${dependency}" "${bundle_real}/" position)
    if(NOT position EQUAL 0)
        message(FATAL_ERROR "Dependency outside the bundle: ${dependency}")
    endif()
endforeach()
