# Directories
RFID_AUTH_WORKING_DIR = ./rfid-plus-display
WIFI_MODULE_WORKING_DIR = ./wifi-module

# Toolchain
TARGET_EXEC := arduino-cli

# Targets
UPLOAD_OP = upload
COMPILE_OP = compile
ESP_TARGET = esp
RFID_TARGET = rfid
ESP_TOOL_TARGET = esptool

# Flags
OBJ := $(eval $(wildcard $(WORKING_DIR)/*.ino))
CONFIG_FILE = --config-file $(WORKING_DIR)/arduino-cli.yaml
JQ_COMMAND = ".detected_ports | . [] |  select( .matching_boards | length > 0) | .port.address" --raw-output
BOARD_PORT = $(shell $(TARGET_EXEC) board list --json | jq -c $(JQ_COMMAND))
ESP_TOOL_PATH = $(WORKING_DIR)/build/data/internal/esp8266_esp8266_*/tools/esptool/esptool.py

# Sets the working directory for the rfid target.
$(RFID_TARGET): 
	@echo "==> Setting the working directory as $(RFID_AUTH_WORKING_DIR) \n"
	$(eval WORKING_DIR := $(RFID_AUTH_WORKING_DIR))

# Sets the working directory for the esp target.
$(ESP_TARGET):
	@echo "==> Setting the working directory as $(WIFI_MODULE_WORKING_DIR) \n"
	$(eval WORKING_DIR := $(WIFI_MODULE_WORKING_DIR))

# Compiles the code in the rfid working directory.
$(COMPILE_OP).$(RFID_TARGET): $(RFID_TARGET) $(OBJ)
	@echo "==> Compiling the code on in $(WORKING_DIR) \n"
	$(TARGET_EXEC) compile $(CONFIG_FILE) $(WORKING_DIR)

# Compiles the code in the esp working directory.
$(COMPILE_OP).$(ESP_TARGET): $(ESP_TARGET) $(OBJ)
	@echo "==> Compiling the code on in $(WORKING_DIR) \n"	
	$(TARGET_EXEC) compile $(CONFIG_FILE) $(WORKING_DIR)

# Builds and Flashes the code in the rfid working directory to the arduino board.
$(UPLOAD_OP).$(RFID_TARGET): $(RFID_TARGET) $(OBJ)
	@echo "==> Uploading the code on to Arduino on port $(BOARD_PORT) \n"
	$(TARGET_EXEC) upload -p $(BOARD_PORT) $(CONFIG_FILE) $(WORKING_DIR)

# Builds and Flashes the code in the esp working directory to the arduino board.
$(UPLOAD_OP).$(ESP_TARGET):  $(ESP_TARGET) $(OBJ)
	@echo "==> Uploading the code on to Arduino on port $(BOARD_PORT) \n"
	$(TARGET_EXEC) upload -p $(BOARD_PORT) $(CONFIG_FILE) $(WORKING_DIR)

# Following the bug fixes described here:
# https://github.com/espressif/esptool/issues/972#issuecomment-2054169803
# the default esptool.py file must be replaced with updated-esptool.py file.
# make esptool should only be run after make compile.esp.
$(ESP_TOOL_TARGET): $(ESP_TARGET)
	@echo "==> Replacing the default esptool.py file with updated-esptool.py contents"
	cp $(WORKING_DIR)/updated-esptool.py $(ESP_TOOL_PATH)