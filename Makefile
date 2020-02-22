OUT := xtmon
INSTALL_DIR := /usr/local/bin

LDLIBS := -lxcb


$(OUT):

.PHONY: debug
debug: CFLAGS += -O0 -g
debug: $(OUT)

.PHONY: clean
clean:
	rm "$(OUT)"

.PHONY: install
install: $(OUT)
	install "$(OUT)" "$(INSTALL_DIR)"

.PHONY: uninstall
uninstall:
	rm "$(INSTALL_DIR)/$(OUT)"
