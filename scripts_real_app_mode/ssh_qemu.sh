#!/bin/bash

# Change HOST_IP and USER
ID="1"
HOST_IP="11.22.33.44"
USER="pnmsimulator"

ssh -p $((2028 + ${ID})) ${USER}@${HOST_IP}
