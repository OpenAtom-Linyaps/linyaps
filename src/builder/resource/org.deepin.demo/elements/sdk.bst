kind: import
description: Import the base deepin SDK

sources:
- kind: ostree
  url: http://10.20.52.7/deepin-sdk/tmp3/
  track: runtime/deepin-bootstrap/x86_64/20.5
  ref: 3e1c82d7e53af284d37c5fce8fef86f89db4d20a02796b6807d2a6bfc10957ca

config:
  source: files
  #target: usr

public:
  bst:
    split-rules:
      filter:
      - "/usr/share/gnu-config/**"