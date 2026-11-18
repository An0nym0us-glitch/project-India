# ---- Core simulation library (no widgets) ----
file(GLOB ISIM_CORE_SOURCES CONFIGURE_DEPENDS
    core/*.cpp gis/*.cpp catalog/*.cpp sim/*.cpp engines/*.cpp advisor/*.cpp
    comparison/*.cpp scenario/*.cpp ai/*.cpp)
file(GLOB ISIM_CORE_HEADERS CONFIGURE_DEPENDS
    core/*.h gis/*.h catalog/*.h sim/*.h engines/*.h advisor/*.h comparison/*.h scenario/*.h ai/*.h)

add_library(isim_core STATIC ${ISIM_CORE_SOURCES} ${ISIM_CORE_HEADERS})
target_include_directories(isim_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(isim_core PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Sql Qt6::Concurrent)
target_compile_definitions(isim_core PUBLIC ISIM_SOURCE_DATA_DIR="${CMAKE_SOURCE_DIR}/data"
    ISIM_VERSION="${PROJECT_VERSION}")
if(WIN32)
  target_link_libraries(isim_core PUBLIC advapi32)  # Windows Credential Manager
endif()

# ---- Desktop application ----
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ui)
  file(GLOB ISIM_UI_SOURCES CONFIGURE_DEPENDS ui/*.cpp ui/*.h app/*.cpp)
  if(ISIM_UI_SOURCES)
    qt_add_executable(IndiaDevelopmentSimulator WIN32 ${ISIM_UI_SOURCES} app/resources.qrc)
    target_link_libraries(IndiaDevelopmentSimulator PRIVATE isim_core Qt6::Widgets Qt6::Charts)
    set_target_properties(IndiaDevelopmentSimulator PROPERTIES OUTPUT_NAME "IndiaDevelopmentSimulator")
    if(WIN32 AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/app/app.rc)
      target_sources(IndiaDevelopmentSimulator PRIVATE app/app.rc)
    endif()
    # copy data next to the executable after build
    add_custom_command(TARGET IndiaDevelopmentSimulator POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:IndiaDevelopmentSimulator>/data)
    install(TARGETS IndiaDevelopmentSimulator RUNTIME DESTINATION .)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/data DESTINATION .)
  endif()
endif()
