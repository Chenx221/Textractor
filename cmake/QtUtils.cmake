macro(msvc_registry_search)
	if(NOT DEFINED Qt6_DIR)
		if (NOT EXISTS ${QT_ROOT})
			# look for user-registry pointing to qtcreator
			get_filename_component(QT_ROOT [HKEY_CURRENT_USER\\Software\\Classes\\Applications\\QtProject.QtCreator.pro\\shell\\Open\\Command] PATH)

			# get root path
			string(REPLACE "/Tools" ";" QT_ROOT "${QT_ROOT}")
			list(GET QT_ROOT 0 QT_ROOT)
		endif()

		set(QT_VERSION 6.10.3)
		set(QT_MSVC 2022)

		if(QT_MSVC)
			if(CMAKE_CL_64)
				SET(QT_SUFFIX "_64")
			else()
				set(QT_SUFFIX "")
			endif()
			set(Qt6_DIR "${QT_VERSION}/msvc${QT_MSVC}${QT_SUFFIX}/lib/cmake/Qt6")
		endif()
	endif()
endmacro()

macro(find_qt6)
	set(CMAKE_INCLUDE_CURRENT_DIR ON)
	#set(CMAKE_AUTOMOC ON)
	set(CMAKE_AUTOUIC ON)
	#add_definitions(-DQT_DEPRECATED_WARNINGS -DQT_DISABLE_DEPRECATED_BEFORE=0x060000)
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(Qt6_DIR "C:/Qt/6.10.3/msvc2022_64/lib/cmake/Qt6")
	else()
		set(Qt6_DIR "C:/Qt/6.10.3/msvc2022/lib/cmake/Qt6")
	endif()
	find_package(Qt6 COMPONENTS ${ARGN})

	if(Qt6_FOUND)
		if(WIN32 AND TARGET Qt6::qmake AND NOT TARGET Qt6::windeployqt)
			get_target_property(_qt6_qmake_location Qt6::qmake IMPORTED_LOCATION)

			execute_process(
				COMMAND "${_qt6_qmake_location}" -query QT_INSTALL_PREFIX
				RESULT_VARIABLE return_code
				OUTPUT_VARIABLE qt6_install_prefix
				OUTPUT_STRIP_TRAILING_WHITESPACE
			)
			set(qt6_install_prefix "${qt6_install_prefix}" CACHE INTERNAL "Qt6 Installation Prefix")
			set(imported_location "${qt6_install_prefix}/bin/windeployqt.exe")
			message(STATUS "QT6_INSTALL_PREFIX FOUND: ${qt6_install_prefix}")
			message(STATUS "WINDEPLOYQT_PATH: ${imported_location}")

			if(EXISTS ${imported_location})
				add_executable(Qt6::windeployqt IMPORTED)

				set_target_properties(Qt6::windeployqt PROPERTIES
					IMPORTED_LOCATION ${imported_location}
				)
			endif()
		endif()
	else()
		message(FATAL_ERROR "Cannot find QT6!")
	endif()
endmacro(find_qt6)
