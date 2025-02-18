add_subdirectory(common)

# Add MpvWidget
add_library(
    mpvwidget
    mpvwidget.cpp
)
set(
    MPV_WIDGET_LIBS
    ${MPV_LIB}
    utils
    Qt5::Widgets
)
if(UNIX AND NOT APPLE)
    set(
        MPV_WIDGET_LIBS
	    ${MPV_WIDGET_LIBS}
	    Qt5::DBus
        Qt5::X11Extras
    )
elseif(UNIX AND APPLE)
    set(
        MPV_WIDGET_LIBS
        ${MPV_WIDGET_LIBS}
        "-framework IOKit -framework CoreFoundation"
    )
endif()
target_link_libraries(
    mpvwidget
    ${MPV_WIDGET_LIBS}
)

# Add About Window
add_library(
    aboutwindow
    aboutwindow.cpp
)
target_link_libraries(
    aboutwindow
    Qt5::Widgets
    Qt5::Core
)

# Add Subtitle List
add_library(
    subtitlelist
    subtitlelistwidget.cpp
    subtitlelistwidget.ui
)
target_link_libraries(
    subtitlelist
    Qt5::Widgets
    mpvadapter
)

add_subdirectory(settings)
add_subdirectory(definition)
add_subdirectory(overlay)