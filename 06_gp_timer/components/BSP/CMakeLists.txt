set(src_dirs
            LED
            GPTIM)

set(include_dirs
            LED
            GPTIM)

set(requires
            esp_timer
            driver)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)