cmake_minimum_required(VERSION 3.15)
project(ch341prog C)

set(PACKAGE_VERSION "1.1")

if(${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    set(IS_FREEBSD TRUE)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(IS_LINUX TRUE)
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Install prefix" FORCE)
endif()

set(BINDIR ${CMAKE_INSTALL_PREFIX}/bin CACHE PATH "binary install path")
set(SHAREDIR ${CMAKE_INSTALL_PREFIX}/share CACHE PATH "data install path")
set(MANDIR ${SHAREDIR}/man CACHE PATH "man install path")
set(MAN1DIR ${MANDIR}/man1 CACHE PATH "man1 install path")

find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBUSB libusb-1.0)

add_compile_options(-Wall)
add_executable(${PROJECT_NAME} main.c ch341a.c)

target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBUSB_LIBRARIES})
target_include_directories(${PROJECT_NAME} PRIVATE ${LIBUSB_INCLUDE_DIRS})

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${BINDIR}
)

if(IS_LINUX)
    install(FILES 99-ch341a-prog.rules
        DESTINATION /lib/udev/rules.d
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    )

    add_custom_target(install-rules
        COMMAND ${CMAKE_COMMAND} -E copy 
            ${CMAKE_CURRENT_SOURCE_DIR}/99-ch341a-prog.rules 
            /etc/udev/rules.d/99-ch341a-prog.rules
        COMMAND udevadm control --reload-rules
        COMMAND ${CMAKE_COMMAND} -E echo "udev rules installed and reloaded"
        COMMENT "Installing Linux udev rules"
    )

elseif(IS_FREEBSD)
    set(DEVD_CONF_DIR "/usr/local/etc/devd")
    add_custom_target(install-rules
        COMMAND ${CMAKE_COMMAND} -E echo "Installing FreeBSD devd rule..."
        COMMAND mkdir -p ${DEVD_CONF_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/ch341a.conf.freebsd ${DEVD_CONF_DIR}/ch341prog.conf
        COMMAND service devd restart
        COMMAND ${CMAKE_COMMAND} -E echo "devd rule installed and service restarted"
        COMMENT "Installing FreeBSD hardware rules"
    )
endif()

# Debian Specific Targets (Linux Only) ---
if(IS_LINUX)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog
        COMMAND dch --create -v ${PACKAGE_VERSION}-1 --package ${PROJECT_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Creating debian changelog"
    )

    add_custom_target(create-changelog
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog
    )

    add_custom_target(build-deb
        COMMAND dpkg-buildpackage -b -us -uc
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Building Debian package"
    )

    add_custom_target(deb
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target build-deb
    )
endif()

add_custom_target(dist
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
    COMMAND ${CMAKE_COMMAND} -E tar cfz 
        ${PROJECT_NAME}-${PACKAGE_VERSION}.tar.gz 
        --format=gnutar 
        -- ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Creating source distribution"
)

message(STATUS "--------------------------------------------")
message(STATUS "Project:         ${PROJECT_NAME}")
message(STATUS "System:          ${CMAKE_SYSTEM_NAME}")
message(STATUS "Version:         ${PACKAGE_VERSION}")
message(STATUS "Install prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Libusb Includes: ${LIBUSB_INCLUDE_DIRS}")
message(STATUS "Libusb Libs:     ${LIBUSB_LIBRARIES}")
message(STATUS "--------------------------------------------")
