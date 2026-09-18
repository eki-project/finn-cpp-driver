#!/bin/bash
# Try to generate stubs and silently handle the output. Logs are only kept if generation failed.
LOGNAME="stubgenlog.txt"
cd ${BINDINGS_DIR}
${PYBIND11_STUBGEN} -o . finnhpcpy.finnhpcpy > "$LOGNAME" 2>&1
if [ $? -ne 0 ]; then
    echo "Stub generation failed! Check logs at $(pwd)/$LOGNAME."
else
    # Delete unnecessary log
    rm $LOGNAME
fi