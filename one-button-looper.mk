######################################
#
# one-button-looper
#
######################################

# where to find the source code - locally in this case
ONE_BUTTON_LOOPER_SITE_METHOD = local
ONE_BUTTON_LOOPER_SITE = $($(PKG)_PKGDIR)/

# even though this is a local build, we still need a version number
# bump this number if you need to force a rebuild
ONE_BUTTON_LOOPER_VERSION = 1

# dependencies (list of other buildroot packages, separated by space)
ONE_BUTTON_LOOPER_DEPENDENCIES =

# LV2 bundles that this package generates (space separated list)
ONE_BUTTON_LOOPER_BUNDLES = kaoss-obl.lv2

# toolchain PATH trimmed to avoid inheriting Windows host entries with spaces
ONE_BUTTON_LOOPER_TOOLCHAIN_PATH = $(HOST_DIR)/bin:$(HOST_DIR)/sbin:$(HOST_DIR)/usr/bin:$(HOST_DIR)/usr/sbin:/usr/bin:/bin

# Use standard variables for cross-compilation
ONE_BUTTON_LOOPER_TARGET_MAKE = PATH="$(ONE_BUTTON_LOOPER_TOOLCHAIN_PATH)" \
    CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_LD)" \
    PKG_CONFIG="$(TARGET_PKG_CONFIG)" \
    CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" \
    LDFLAGS="$(TARGET_LDFLAGS)" \
    $(MAKE) -C $(@D)/source

# build command
define ONE_BUTTON_LOOPER_BUILD_CMDS
    $(ONE_BUTTON_LOOPER_TARGET_MAKE)
endef

# install command
define ONE_BUTTON_LOOPER_INSTALL_TARGET_CMDS
    $(ONE_BUTTON_LOOPER_TARGET_MAKE) install DESTDIR=$(TARGET_DIR)
endef


# import everything else from the buildroot generic package
$(eval $(generic-package))