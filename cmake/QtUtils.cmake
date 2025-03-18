macro(msvc_registry_search)
	if(NOT DEFINED Qt5_DIR)
		if (NOT EXISTS ${QT_ROOT})
			# look for user-registry pointing to qtcreator
			get_filename_component(QT_ROOT [HKEY_CURRENT_USER\\Software\\Classes\\Applications\\QtProject.QtCreator.pro\\shell\\Open\\Command] PATH)

			# get root path
			string(REPLACE "/Tools" ";" QT_ROOT "${QT_ROOT}")
			list(GET QT_ROOT 0 QT_ROOT)
		endif()

		set(QT_VERSION 5.13.2)
		set(QT_MSVC 2019)

		if(QT_MSVC)
			if(CMAKE_CL_64)
				SET(QT_SUFFIX "_64")
			else()
				set(QT_SUFFIX "")
			endif()
			set(Qt5_DIR "${QT_VERSION}/msvc2017${QT_SUFFIX}/lib/cmake/Qt5")
		endif()
	endif()
endmacro()

macro(find_qt5)
	set(CMAKE_INCLUDE_CURRENT_DIR ON)
	#set(CMAKE_AUTOMOC ON)
	set(CMAKE_AUTOUIC ON)
	#add_definitions(-DQT_DEPRECATED_WARNINGS -DQT_DISABLE_DEPRECATED_BEFORE=0x060000)
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(Qt5_DIR "C:/Qt/Qt5.13.2/5.13.2/msvc2017_64/lib/cmake/Qt5")
	else()
		set(Qt5_DIR "C:/Qt/Qt5.13.2/5.13.2/msvc2017/lib/cmake/Qt5")
	endif()
	find_package(Qt5 COMPONENTS ${ARGN})

	if(Qt5_FOUND)
		if(WIN32 AND TARGET Qt5::qmake AND NOT TARGET Qt5::windeployqt)
			get_target_property(_qt5_qmake_location Qt5::qmake IMPORTED_LOCATION)

			execute_process(
				COMMAND "${_qt5_qmake_location}" -query QT_INSTALL_PREFIX
				RESULT_VARIABLE return_code
				OUTPUT_VARIABLE qt5_install_prefix
				OUTPUT_STRIP_TRAILING_WHITESPACE
			)

			set(imported_location "${qt5_install_prefix}/bin/windeployqt.exe")

			if(EXISTS ${imported_location})
				add_executable(Qt5::windeployqt IMPORTED)

				set_target_properties(Qt5::windeployqt PROPERTIES
					IMPORTED_LOCATION ${imported_location}
				)
			endif()
		endif()
	else()
		message(FATAL_ERROR "Cannot find QT5!")
	endif()
endmacro(find_qt5)
