build app solver:
    #!/usr/bin/env sh
    make -C maxsat/{{ solver }}
    IPAMIRSOLVER="{{ solver }}" make -C app/{{ app }}
