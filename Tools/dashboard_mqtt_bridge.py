"""Compatibility entry point. See --help for HTTP/MQTT logging and dashboard proxy."""
from logger_v3 import main
if __name__ == '__main__':
    raise SystemExit(main())
