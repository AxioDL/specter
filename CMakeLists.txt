add_subdirectory(freetype2)
if (NOT MSVC)
  target_compile_options(freetype PRIVATE -Wno-implicit-fallthrough)
endif()

add_subdirectory(zeus)
set(ZEUS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/zeus/include)

add_subdirectory(resources/fonts)

add_library(specter
  lib/Button.cpp
  lib/Control.cpp
  lib/FileBrowser.cpp
  lib/FontCache.cpp
  lib/Icon.cpp
  lib/Menu.cpp
  lib/MessageWindow.cpp
  lib/ModalWindow.cpp
  lib/MultiLineTextView.cpp
  lib/PathButtons.cpp
  lib/RootView.cpp
  lib/ScrollView.cpp
  lib/Space.cpp
  lib/SplitView.cpp
  lib/Table.cpp
  lib/TextField.cpp
  lib/TextView.cpp
  lib/Toolbar.cpp
  lib/Tooltip.cpp
  lib/View.cpp
  lib/ViewResources.cpp

  include/specter/Button.hpp
  include/specter/Control.hpp
  include/specter/FileBrowser.hpp
  include/specter/FontCache.hpp
  include/specter/IMenuNode.hpp
  include/specter/IViewManager.hpp
  include/specter/Icon.hpp
  include/specter/Menu.hpp
  include/specter/MessageWindow.hpp
  include/specter/ModalWindow.hpp
  include/specter/MultiLineTextView.hpp
  include/specter/Node.hpp
  include/specter/NodeSocket.hpp
  include/specter/NumericField.hpp
  include/specter/Outliner.hpp
  include/specter/Panel.hpp
  include/specter/PathButtons.hpp
  include/specter/RootView.hpp
  include/specter/ScrollView.hpp
  include/specter/Space.hpp
  include/specter/SplitView.hpp
  include/specter/Table.hpp
  include/specter/TextField.hpp
  include/specter/TextView.hpp
  include/specter/Toolbar.hpp
  include/specter/Tooltip.hpp
  include/specter/View.hpp
  include/specter/ViewResources.hpp
  include/specter/genie.hpp
  include/specter/specter.hpp
)

if (MSVC)
  target_compile_options(specter PRIVATE
    # Enforce various standards compliant behavior.
    $<$<COMPILE_LANGUAGE:CXX>:/permissive->

    # Enable standard volatile semantics.
    $<$<COMPILE_LANGUAGE:CXX>:/volatile:iso>

    # Reports the proper value for the __cplusplus preprocessor macro.
    $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>

    # Use latest C++ standard.
    $<$<COMPILE_LANGUAGE:CXX>:/std:c++latest>
  )
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    # Flags for MSVC (not clang-cl)
    target_compile_options(specter PRIVATE
      # Allow constexpr variables to have explicit external linkage.
      $<$<COMPILE_LANGUAGE:CXX>:/Zc:externConstexpr>

      # Assume that new throws exceptions, allowing better code generation.
      $<$<COMPILE_LANGUAGE:CXX>:/Zc:throwingNew>
    )
  endif()
endif()

target_link_libraries(specter PUBLIC
  freetype
  hecl-full
  specter-fonts
  zeus
  UrdeLocales
)

target_include_directories(specter PUBLIC include freetype2/include)
target_atdna(specter atdna_FontCache.cpp include/specter/FontCache.hpp)
