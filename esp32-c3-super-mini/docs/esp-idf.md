Files: none (reference doc)

How the ESP32-C3 firmware works in C with ESP-IDF, written for someone coming from the Python debug driver.


<python to C>
| Python                      | C / ESP-IDF                                                    |
|-----------------------------|----------------------------------------------------------------|
| class with methods          | one .c file with functions sharing file-level static variables |
| \_private method            | static function                                                |
| raise Exception             | return an esp_err_t code                                       |
| return None                 | return an error code like ESP_ERR_TIMEOUT                      |
| returning data              | caller passes a buffer and a pointer the function fills in     |
| log.info(...)               | ESP_LOGI(TAG, ...)                                             |
| asyncio.to_thread / threads | FreeRTOS task                                                  |
| time.sleep                  | vTaskDelay                                                     |
| serial.Serial               | uart_driver_install + uart_read_bytes / uart_write_bytes       |


<project layout>
ESP-IDF builds with CMake. It expects a top-level `CMakeLists.txt` (which pulls in IDF and names the project) and one folder per component, each with its own `CMakeLists.txt` that lists its `.c` files and include folders.
By default the app's code lives in a component called `main/`. Our code is in `src/`, so either rename it to `main/` or tell IDF about it in the top-level file.
!! the file must be named `CMakeLists.txt` exactly. `src/CMakeList.txt` (missing the s) is ignored by CMake.
`sdkconfig` is the generated settings file from menuconfig. Commit it (or a trimmed `sdkconfig.defaults`) so everyone builds with the same settings. `build/` is never committed.


<headers and source files>
`.h` is the public interface: types, defines and function declarations other files may use. `.c` holds the actual code.
Other files include the `.h`, never the `.c`. `#pragma once` stops a header being pulled in twice.
`static` on a function or a file-level variable makes it private to that `.c` file. `static` on a variable inside a function makes it keep its value between calls.
`const` data stays in flash instead of RAM, which matters for big tables like `RADAR_CFG`.
`volatile` tells the compiler a variable can change behind its back (another task, an interrupt), so it re-reads it every time.
Fixed-size integers (`uint8_t`, `uint32_t`, ...) come from `stdint.h`; use them for anything that matches a wire format, like the radar header.


<entry point>
There is no `main()`. IDF boots, starts FreeRTOS, then calls `app_main()` from `main.c` in its own task.
`app_main()` may return; that only ends its own task. Long-running work (the radar read loop, the supervisor) goes in tasks it creates.


<errors>
Nothing throws. Functions return an `esp_err_t`: `ESP_OK` (0) on success, otherwise a named code (`ESP_FAIL`, `ESP_ERR_TIMEOUT`, `ESP_ERR_NO_MEM`, ...).
`ESP_ERROR_CHECK(call)` reboots the chip with a message if the call fails. Fine during init, wrong for anything that can fail normally (a radar read timeout).
`ESP_RETURN_ON_ERROR` from `esp_check.h` passes a failure up to the caller, which is what the drivers should mostly do.
`esp_err_to_name(err)` turns a code into text for logs.
Data comes back through pointers the caller passes in, e.g. `awr1843_read` fills `buf` and `*len`.


<logging>
`ESP_LOGE / W / I / D / V(TAG, "format", ...)` = error, warning, info, debug, verbose. `TAG` is a per-file string (`"awr1843"`), like the logger name in Python.
Default level and where output goes (UART0 or USB Serial/JTAG) are set in menuconfig. `esp_log_level_set(TAG, level)` changes it per tag at runtime.
!! once GPIO18/19 become radar UART pins, USB Serial/JTAG is gone, so the console must go to a free UART or be turned off.


<FreeRTOS>
A task is a function that loops forever, started with `xTaskCreate` (name, stack size, priority).
!! in IDF the stack size is in bytes, not words as in plain FreeRTOS docs. Too small a stack crashes with a "stack overflow" panic.
The C3 has a single core, so the `PinnedToCore` variants don't matter.
`vTaskDelay(pdMS_TO_TICKS(ms))` sleeps. The default tick is 100 Hz, so delays round to 10 ms.
Tasks talk through queues (`xQueueSend` / `xQueueReceive`) and protect shared things with mutexes (`xSemaphoreCreateMutex`, take / give).


<memory>
No garbage collector. `malloc` / `free` exist, but allocate buffers once (static or at init) and reuse them; allocating per frame fragments the ~300 KB of usable RAM.
`heap_caps_get_free_size` shows how much heap is left.


<UART>
Three calls to set one up: `uart_param_config` (baud, bits, clock source), `uart_set_pin` (which GPIOs; any pin works through the GPIO matrix), `uart_driver_install` (RX/TX buffer sizes).
Then `uart_write_bytes`, `uart_read_bytes` (with a timeout in ticks) and `uart_flush_input` (= `reset_input_buffer`).
The C3 has UART0 and UART1 only.


<build and flash>
Load the toolchain in every new shell with IDF's `export.sh` (or `export.fish` for fish).
| Command                        | What it does                                                                  |
|--------------------------------|-------------------------------------------------------------------------------|
| idf.py set-target esp32c3      | Once per project. Picks the chip and creates sdkconfig                        |
| idf.py menuconfig              | Text UI for sdkconfig (console output, log level, FreeRTOS tick rate etc.)    |
| idf.py build                   | Compiles everything into build/                                               |
| idf.py -p /dev/ttyACM0 flash   | Writes the firmware to the board                                              |
| idf.py -p /dev/ttyACM0 monitor | Serial console with log output. Exit with Ctrl+\]                             |
| idf.py fullclean               | Deletes build/ when the build gets confused (e.g. after changing CMake files) |

!! with GPIO18/19 used for the radar, the board no longer shows up over USB while the firmware runs. Hold BOOT while plugging in / resetting to flash.
