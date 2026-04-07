# Build Error / Warning Fix Checklist

- Target: `rtl8730e/loadable_ext_ddr_st7785`
- Source log: `build.log`
- Last verified build: `2026-04-07`
- Final status: build succeeds with `TOTAL=0`, `UNIQUE=0`
- Note: earlier over-edits were reviewed again, unnecessary `FAR` additions were removed, and only warning/error-related changes were kept.

## Current Log Check

- [x] `build.log` contains no `warning:`
- [x] `build.log` contains no `error:`
- [x] `build.log` contains no `undefined reference to`
- [x] Package generation and header verification completed successfully

## Over-edits Reverted

- [x] `os/drivers/lcd/st7785.c`: earlier `FAR`-only prototype change was removed completely because it was not needed for the warning fix.
- [x] `apps/examples/lcd_test/example_lcd.c`: the first `disp_xres/disp_yres` rename was not kept; the final retained rename is only `width/height`, which is the minimal change that still fixes the real `-Wshadow` warning.
- [x] `os/drivers/lcd/mipi_lcd.c`, `os/drivers/lcd/mipi_lcd.h`, `os/include/tinyara/lcd/st7785.h`, `os/include/tinyara/audio/ndp120.h`, `os/board/rtl8730e/src/rtl8730e_ndp120.c`, `os/drivers/ai-soc/ndp120/src/ndp120_api.c`, `os/drivers/audio/ndp120_voice.c`, `os/binfmt/binfmt_execmodule.c`, `os/kernel/wdog/wd_start.c`, `os/pm/pm_procfs.c`: newly added `FAR` qualifiers were removed while keeping the actual type/prototype fixes.

## Retained File-by-File Reasons

- [x] `apps/examples/lcd_test/example_lcd.c`: `power_cycle_test` and `frame_change_test` were changed to proper task entry signatures, unused locals were removed, and `prepare_frame_buffer` parameters were renamed to `width`/`height` to fix the real `-Wshadow` warnings against the file-scope `xres`/`yres`.
- [x] `apps/examples/lcd_test/lcd_verification.c`: removed unused globals and locals that produced `-Wunused-variable` warnings.
- [x] `apps/examples/power/power_main.c`: changed PM domain variables to `struct pm_domain_s *` because the PM ioctl path returns and consumes pointers, not integer IDs.
- [x] `apps/shell/tash_internal.h`: updated `tash_pm_get_domain_id()` prototype to return `struct pm_domain_s *` so the declaration matches the implementation and call sites.
- [x] `apps/shell/tash_pm.c`: stored the TASH PM domain as `struct pm_domain_s *` and used `NULL` as the sentinel, matching the PM driver API and removing pointer/integer mismatch warnings.
- [x] `apps/system/init/Makefile`: split dependency generation between `$(CC) $(CFLAGS)` for C sources and `$(CXX) $(CXXFLAGS)` for `terminate_handler.cxx`, which removes the `cc1plus` warnings about C-only options.
- [x] `apps/system/utils/security_level_cmd.c`: changed the TASH callback to return `int` and added explicit status returns so the command registration matches the expected callback type.
- [x] `apps/examples/wifi_manager/wm_test/wm_test_connectbyrssi_test.c`: changed `_wt_disconnect()` to `_wt_disconnect(void)` so the file no longer emits the strict-prototypes warning for an old-style empty parameter list.
- [x] `external/cmsis_dsp/Makefile`: preserved and re-archived object files correctly during rebuilds and removed stale `.o` files during clean, which prevents archive/update build failures during clean rebuild flows.
- [x] `external/cmsis_nn/Makefile`: same archive/update cleanup fix as `external/cmsis_dsp/Makefile` for clean rebuild stability.
- [x] `external/libopus/celt/arch.h`: changed `#elif OPUS_ARM_INLINE_EDSP` to a `defined()`-guarded form to eliminate the undefined macro warning.
- [x] `external/libopus/celt/mathops.h`: wrapped `PI` with `#ifndef PI` so the header no longer redefines an existing macro.
- [x] `external/libopus/silk/SigProc_FIX.h`: changed `#if EMBEDDED_ARM` to a `defined()`-guarded form to eliminate the undefined macro warning.
- [x] `external/mbedtls/alt/dhm_alt.c`: fixed wrong pointer levels, assignment-precedence expressions, and uninitialized locals that were producing incompatible pointer, parentheses, and uninitialized-variable warnings.
- [x] `external/onert-micro/onert-micro/luci-interpreter/include/luci_interpreter/core/Tensor.h`: cast the signed index to `uint32_t` before comparing with `const_dims.size()` to remove the signed/unsigned comparison warning.
- [x] `external/onert-micro/onert-micro/luci-interpreter/src/kernels/Builders.h`: guarded `USE_GENERATED_LIST` with `defined()` so missing macro definitions no longer warn.
- [x] `external/onert-micro/onert-micro/luci-interpreter/src/kernels/KernelBuilder.h`: same `defined(USE_GENERATED_LIST)` guard fix as `Builders.h`.
- [x] `external/onert-micro/onert-micro/luci-interpreter/pal/common/PALReduceCommon.h`: changed the reduction loop index to `int` so it matches the signed `output_flat_size` argument and removes the signed/unsigned comparison warning.
- [x] `external/onert-micro/onert-micro/luci-interpreter/src/core/RuntimeGraph.cpp`: reordered constructor initializers and changed multiple graph traversal indices to the correct unsigned FlatBuffers types, removing initializer-order and signed/unsigned warnings.
- [x] `external/onert-micro/onert-micro/luci-interpreter/src/kernels/Add.cpp`: parenthesized the mixed `or`/`&&` assert condition to remove the operator-precedence warning.
- [x] `external/onert-micro/onert-micro/luci-interpreter/src/kernels/Concatenation.cpp`: changed the input loop index to `flatbuffers::uoffset_t` so it matches `inputs()->size()`.
- [x] `external/onert-micro/onert-micro/luci-interpreter/src/kernels/Fill.cpp`: changed the fill loop index to `size_t` so it matches the `flat_size` parameter type.
- [x] `external/onert-micro/onert-micro/luci-interpreter/src/kernels/FullyConnected.cpp`: changed the loop index to `uint32_t` so it matches `num_dims`.
- [x] `external/onert-micro/onert-micro/luci-interpreter/src/kernels/Gather.cpp`: removed the unused temporary `x` variable that triggered `-Wunused-variable`.
- [x] `external/onert-micro/onert-micro/luci-interpreter/src/kernels/Mul.cpp`: parenthesized the mixed `or`/`&&` assert condition to remove the operator-precedence warning.
- [x] `external/onert-micro/onert-micro/luci-interpreter/src/kernels/Pack.cpp`: changed the dimension loop index to `uint32_t` so it matches `input_dims.size()`.
- [x] `external/onert-micro/onert-micro/luci-interpreter/src/kernels/Unpack.cpp`: changed loop indices and size comparisons to typed casts that match the FlatBuffers/API return types, removing signed/unsigned warnings.
- [x] `external/onert-micro/onert-micro/luci-interpreter/src/kernels/Utils.cpp`: removed the duplicate unused quantized activation-range helper overload that produced an unused-function warning.
- [x] `external/onert-micro/onert-micro/luci-interpreter/src/kernels/Utils.h`: changed tensor-shape loop indices to `uint32_t` so they match the size-returning helpers.
- [x] `external/onert-micro/onert-micro/luci-interpreter/src/kernels/While.cpp`: changed runtime-graph size variables and loop indices to consistent typed values/casts so the while-kernel checks and loops no longer warn.
- [x] `external/onert-micro/onert-micro/luci-interpreter/src/loader/GraphLoader.cpp`: changed FlatBuffers input iteration and distance handling to typed unsigned values, removing signed/unsigned comparison warnings.
- [x] `framework/src/aifw/AIManifestParser.cpp`: removed the outer `stdVals` declaration that was shadowed and unused.
- [x] `framework/src/aifw/AIModel.cpp`: reordered and completed constructor initializer lists to match member declaration order and remove constructor initialization-order warnings.
- [x] `framework/src/aifw/AIModelService.cpp`: logged `errno` directly instead of keeping an unused local copy.
- [x] `framework/src/ble_manager/ble_manager_state.c`: removed the unused `conn` variable.
- [x] `framework/src/media/voice/EPDProcessHandler.cpp`: removed the unused `srawData` variable.
- [x] `os/arch/arm/src/amebasmart/amebasmart_enet.c`: zero-initialized `packet_filter` to remove the maybe-uninitialized warning.
- [x] `os/arch/arm/include/amebasmart/cmsis_nn/arm_nn_math_types.h`: guarded `__ARM_FEATURE_MVE` with `defined()` so the CMSIS-NN header no longer warns when MVE support macros are absent.
- [x] `os/arch/arm/src/amebasmart/amebasmart_i2s.c`: changed hardware config pointers to `const` and removed unnecessary casts so the code no longer discards `const`.
- [x] `os/arch/arm/src/armv7-a/arm_fullcontextrestore.c`: added `__builtin_unreachable()` after the restore syscall because control never really returns there.
- [x] `os/binfmt/binfmt_execmodule.c`: cast `register_exidx()` arguments to the expected unwind and text-pointer types.
- [x] `os/board/rtl8730e/include/board_pins.h`: replaced `NULL` with `NC` for `I2S2_MCLK`, which is the proper pin sentinel and avoids pointer/integer misuse.
- [x] `os/board/rtl8730e/src/component/bluetooth/example/ble_peripheral/ble_tizenrt_server.c`: added braces around the one-line `if` blocks to fix the misleading-indentation warnings.
- [x] `os/board/rtl8730e/src/component/bluetooth/example/ble_scatternet/ble_tizenrt_combo.c`: fixed the invalid `||` null-check logic, added the missing return value, and removed an unused static variable.
- [x] `os/board/rtl8730e/src/component/lwip/api/lwip_netconf.c`: stored the cached IPv4 address as `struct in_addr` and returned it through the correct typed pointer, removing incompatible pointer and discarded-volatile warnings.
- [x] `os/board/rtl8730e/src/component/mbed/targets/hal/rtl8730e/i2s_api.c`: cast the `memcpy` source to `const void *`, marked the unused parameter, and removed the unused local.
- [x] `os/board/rtl8730e/src/component/soc/amebad2/fwlib/ap_peripheral/ameba_loguart.c`: removed the unused `CounterIndex` variable.
- [x] `os/board/rtl8730e/src/component/soc/amebad2/fwlib/ram_common/ameba_rom_patch.c`: removed the unused `TAG` variable.
- [x] `os/board/rtl8730e/src/component/wifi/inic/inic_ipc_host_api.c`: removed `volatile` from the cached IP array so `memcmp` and `rtw_memcpy` no longer discard qualifiers.
- [x] `os/board/rtl8730e/src/rtl8730e_boot.c`: included `<tinyara/i2c.h>`, removed an unnecessary cast on `up_i2cinitialize()`, and added the missing return in `up_check_iwdg()`.
- [x] `os/board/rtl8730e/src/rtl8730e_ndp120.c`: aligned the attach callback signature with the lower-half API, preserved the callback argument explicitly, passed the correct integer argument to `gpio_irq_init()`, consumed unused IRQ parameters, and changed `rtl8730e_ndp120_reset()` to a real prototype.
- [x] `os/drivers/ai-soc/ndp120/include/syntiant_ilib/syntiant_ndp_ilib_version.h`: guarded and normalized the `STR`/`STR_HELPER` macros so the header no longer redefines them.
- [x] `os/drivers/ai-soc/ndp120/src/ndp120_api.c`: converted the HPWORK handler to a `void (*)(void *)`-compatible worker, kept error logging, and queued it with the correct argument type.
- [x] `os/drivers/audio/ndp120_voice.c`: changed the interrupt dispatch callback to the same `void *` callback shape used by the lower-half attach path.
- [x] `os/drivers/audio/syu645b.c`: added the missing `return ret;` in a non-void function.
- [x] `os/drivers/compression/compress.c`: removed the useless `static` storage-class specifier from the struct type definition.
- [x] `os/drivers/lcd/lcd_dev.c`: fixed the flush-thread area pointer to use the actual stored pointer type, removed the unused local, and passed `planeinfo` with the correct pointer type.
- [x] `os/drivers/lcd/mipi_lcd.c`: corrected helper function signatures, used typed `uint16_t` buffer pointers for rotation, made init-command tables `const`, and removed bad function-pointer casts.
- [x] `os/drivers/lcd/mipi_lcd.h`: added the forward declaration for `struct mipi_lcd_dev_s` and aligned helper prototypes with the fixed implementation signatures.
- [x] `os/include/aio.h`: forward-declared `struct aiocb` so generated proxy prototypes do not warn about an incomplete struct declared only inside parameter lists.
- [x] `os/include/tinyara/audio/audio.h`: removed `packed_struct` from `struct ap_buffer_s` so semaphore/queue member address usage no longer triggers packed-member alignment warnings.
- [x] `os/include/tinyara/audio/ndp120.h`: changed the lower-half `attach` argument from `char *` to `void *` so the callback interface matches how the driver really passes opaque context pointers.
- [x] `os/include/tinyara/lcd/lcd.h`: added a forward declaration for `struct lcd_info_s` so related LCD prototypes are visible without a parameter-list-only struct declaration warning.
- [x] `os/include/tinyara/lcd/st7785.h`: made the vendor init command pointer `const`, matching the read-only table data.
- [x] `os/kernel/log_dump/log_dump.c`: introduced a local `unsigned long` size variable because `compress_block()` expects `long unsigned int *`, not the previous narrower type.
- [x] `os/kernel/wdog/wd_start.c`: cast queue nodes to `struct wdog_s *` when walking the watchdog list to remove incompatible-pointer warnings.
- [x] `os/net/blemgr/bledev.c`: fixed connection-handle types, corrected 16-bit `cid`/`len` reads, corrected the COC value pointer type, and renamed the shadowing `data` local to `payload`.
- [x] `os/net/blemgr/bledev_mgr_server.c`: updated the null connected callback to include the `adv_handle` argument and explicitly marked all unused parameters.
- [x] `os/net/netmgr/netdev_mgr_internal.h`: forward-declared `struct ifaddrs` so the header no longer declares it only inside parameter lists.
- [x] `os/pm/pm_procfs.c`: fixed comment text that contained `/*` inside comments and kept the path-template traversal pointers `const` so qualifier-discard warnings are removed.
- [x] `os/syscall/Makefile`: added `$(MKSYSCALL)` as a `.context` dependency so syscall proxies are regenerated when the generator changes.
- [x] `os/tools/mksyscall.c`: generated `return 0;` in the affected stub cases so auto-generated syscall proxies no longer emit wrong return-type warnings.

## Verified But Not Committed

- [x] `build/configs/rtl8730e/loadable_ext_ddr_st7785/Untitled`: local text artifact containing only `loadable_ext_ddr_st7785`; not source code and not related to build warning fixes.
- [x] `os/.codex`: empty local artifact file; not source code and not related to build warning fixes.
