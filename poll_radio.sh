#!/bin/bash
while true; do
  if [ -S /tmp/mpvsocket ]; then
    TITLE=$(echo '{ "command": ["get_property", "media-title"] }' | socat - /tmp/mpvsocket 2>/dev/null | grep -o '"data":"[^"]*' | cut -d'"' -f4)
    if [ ! -z "$TITLE" ]; then
      echo "$TITLE" > /tmp/radio_meta.txt
    else
      echo "" > /tmp/radio_meta.txt
    fi
  else
    echo "" > /tmp/radio_meta.txt
  fi
  sleep 3
done
