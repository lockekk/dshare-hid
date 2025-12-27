# generic bin2array.cmake script
# Usage: cmake -P bin2array.cmake <input_file> <output_file>

if(NOT EXISTS "${INPUT_FILE}")
    message(FATAL_ERROR "Input file not found: ${INPUT_FILE}")
endif()

file(READ "${INPUT_FILE}" HEX_CONTENT HEX)

string(LENGTH "${HEX_CONTENT}" HEX_LEN)
set(FORMATTED_CONTENT "")
set(BYTES_PER_LINE 16)
set(BYTE_COUNT 0)

math(EXPR STOP "${HEX_LEN} - 1")

foreach(i RANGE 0 ${STOP} 2)
    string(SUBSTRING "${HEX_CONTENT}" ${i} 2 BYTE_HEX)
    string(APPEND FORMATTED_CONTENT "0x${BYTE_HEX}, ")

    math(EXPR BYTE_COUNT "${BYTE_COUNT} + 1")
    if(BYTE_COUNT EQUAL BYTES_PER_LINE)
        string(APPEND FORMATTED_CONTENT "\n")
        set(BYTE_COUNT 0)
    endif()
endforeach()

file(WRITE "${OUTPUT_FILE}" "/* Generated from ${INPUT_FILE} */\n")
file(APPEND "${OUTPUT_FILE}" "${FORMATTED_CONTENT}\n")
