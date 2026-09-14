cmake_minimum_required(VERSION 3.16)

project(ChargingPlatform LANGUAGES CXX)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network Sql)

add_subdirectory(protocol)
add_subdirectory(db)
add_subdirectory(server)
add_subdirectory(client)

# --- Phase 2: big-data dashboard + ML/recommendation backend (plain Python) ---
# bigdata/ and ml/ are not compiled by CMake; their CMakeLists.txt only
# register the .py files as IDE-visible sources. The targets below are
# convenience run-targets for the shared top-level requirements.txt /
# run_api.py — none of them run automatically as part of a normal build.
add_subdirectory(bigdata)
add_subdirectory(ml)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    if(WIN32)
        set(PHASE2_VENV_BIN ${CMAKE_SOURCE_DIR}/.venv/Scripts)
    else()
        set(PHASE2_VENV_BIN ${CMAKE_SOURCE_DIR}/.venv/bin)
    endif()

    add_custom_target(phase2_venv
        COMMAND ${Python3_EXECUTABLE} -m venv ${CMAKE_SOURCE_DIR}/.venv
        COMMAND ${PHASE2_VENV_BIN}/pip install -r ${CMAKE_SOURCE_DIR}/requirements.txt
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Creating .venv and installing requirements.txt for the Phase 2 Python backend"
    )

    add_custom_target(phase2_api
        COMMAND ${PHASE2_VENV_BIN}/uvicorn run_api:app --reload --port 8090
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running the Phase 2 FastAPI service on port 8090 (run phase2_venv first)"
    )
else()
    message(WARNING "Python3 not found; phase2_venv / phase2_api targets are unavailable — run pip/uvicorn manually for bigdata/ and ml/")
endif()