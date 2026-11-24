.PHONY: all firmware deps test audit render clean
ifneq ($(wildcard deps/toolchain/bin/arm-none-eabi-gcc),)
export PATH := $(abspath deps/toolchain/bin):$(PATH)
endif
all: firmware
deps:
	python3 tools/setup.py
firmware:
	$(MAKE) -C firmware
test:
	mkdir -p build
	clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -Ifirmware tests/engine_test.cpp firmware/engine.cpp -o build/engine-test
	./build/engine-test
	clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -Ifirmware tests/loop_splice_test.cpp firmware/engine.cpp -o build/loop-splice-test
	./build/loop-splice-test
	clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -Ifirmware -Ideps/libDaisy/src tests/recovery_logging_test.cpp -o build/recovery-logging-test
	./build/recovery-logging-test
	clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -Ifirmware tests/debug_test.cpp firmware/debug.cpp -o build/debug-test
	./build/debug-test
	python3 tests/control_test.py
audit:
	mkdir -p build
	clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -Ifirmware tests/use_case_audit.cpp firmware/engine.cpp -o build/use-case-audit
	./build/use-case-audit
render:
	mkdir -p build/audio
	clang++ -std=c++17 -O2 -Wall -Wextra -Werror -Ifirmware tools/render.cpp firmware/engine.cpp -o build/render
	./build/render build/audio
clean:
	$(MAKE) -C firmware clean
	rm -rf build
