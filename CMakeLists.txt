cmake_minimum_required(VERSION 3.22)
project(RawDubPrototype VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)

add_subdirectory(../DDD1BassTranslator/JUCE JUCE)

juce_add_gui_app(RawDubPrototype
    COMPANY_NAME "HomeMade"
    PRODUCT_NAME "Raw Dub Prototype"
)

juce_generate_juce_header(RawDubPrototype)

target_sources(RawDubPrototype PRIVATE
    Source/Main.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp
    Source/AudioEngine.h
    Source/AudioEngine.cpp
    Source/KickSynth.h
    Source/KickSynth.cpp
    Source/BassSynth.h
    Source/BassSynth.cpp
    Source/SkankSynth.h
    Source/SkankSynth.cpp
    Source/PointCurve.h
    Source/SkankPatternSlot.h
    Source/StepPattern.h
    Source/StepLevel.h
    Source/Scale.h
    Source/StepButton.h
    Source/StepButton.cpp
    Source/CurveLaneEditor.h
    Source/ProjectIO.h
    Source/ProjectIO.cpp
    Source/BWLookAndFeel.h
)

target_compile_definitions(RawDubPrototype PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(RawDubPrototype PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
