# Header-only wrapper so ESP-IDF can use the library without copying it.
# Paths are relative to this folder:
#   ../../../../../include  -> <repo>/include             (the ecl headers)
#   ../../../../common      -> <repo>/examples/common     (vibration_pipeline.hpp)
idf_component_register(
    INCLUDE_DIRS
        "../../../../../include"
        "../../../../common"
)
