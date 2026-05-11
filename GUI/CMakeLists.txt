add_executable(Textractor WIN32 
	main.cpp
	exception.cpp
	mainwindow.cpp
	extenwindow.cpp
	attachprocessdialog.cpp
	Textractor.rc
	Textractor.ico
		../extensions/network.cpp
)
target_precompile_headers(Textractor REUSE_FROM pch)
target_link_libraries(Textractor host Qt6::Widgets Qt6::Gui shell32 winhttp)

if (NOT EXISTS ${CMAKE_FINAL_OUTPUT_DIRECTORY}/Qt6Core.dll AND NOT EXISTS ${CMAKE_FINAL_OUTPUT_DIRECTORY}/Qt6Cored.dll)
	set(QT_BIN_DIR "${qt6_install_prefix}/bin")
	add_custom_command(TARGET Textractor
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/windeployqt"
		COMMAND set PATH=%PATH%$<SEMICOLON>${qt6_install_prefix}/bin
		COMMAND Qt6::windeployqt --dir ${CMAKE_FINAL_OUTPUT_DIRECTORY} "${CMAKE_FINAL_OUTPUT_DIRECTORY}/Textractor.exe"
	)
endif()
