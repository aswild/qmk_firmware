# enable VIA, but disable the insecure key-matrix testing feature that keychron_common.mk enables
VIA_ENABLE = yes
OPT_DEFS := $(filter-out -DVIA_INSECURE,$(OPT_DEFS))
