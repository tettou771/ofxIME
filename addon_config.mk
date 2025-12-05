meta:
	ADDON_NAME = ofxIME
	ADDON_DESCRIPTION = OS native IME support for Japanese input
	ADDON_AUTHOR = tettou771
	ADDON_URL = https://github.com/tettou771/ofxIME

common:

osx:
	ADDON_LDFLAGS = -framework Carbon

vs:
	ADDON_LIBS = imm32.lib
