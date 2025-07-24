#!/bin/bash

nbfc_lib="./libs/nbfc"
python_lib="./libs/python"

function update_package() {
    echo "Update package"
    sudo apt update
}

function nbfc_lib() {
    while IFS= read -r pkg; do
        if [[ -n "$pkg" ]]; then
            echo "Installing: $pkg"
            sudo apt install -y "$pkg"
        fi
    done < "$nbfc_lib"
}

function install_nbfc() {
    cd ./nbfc-linux || { echo "Cannot enter nbfc-linux folder"; return 1; }
    make
    sudo make install

    echo "Detecting available models..."
    model=$(nbfc get-model-name)

    if ls /usr/share/nbfc/configs | grep -iq "$model"; then
        echo "Model found. Applying configuration for: $model"
        sudo nbfc config --apply "$model"
        sudo nbfc start
        sudo systemctl enable nbfc_service
        echo "NBFC is now running and enabled at boot!"
    else
        echo "Model '$model' not found in configs."
        echo "You may need to manually copy or create a config for your machine."
    fi
}


function pythonn_lib() {
    while IFS= read -r pkg; do
        if [[ -n "$pkg" ]]; then
            echo "Installing: $pkg"
            sudo apt install -y "$pkg"
        fi
    done < "$python_lib"
}


echo "Running NBFC setup . . ."

update_package
install_libs
install_nbfc
pythonn_lib
