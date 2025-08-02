#!/bin/bash

nbfc_lib="./libs/nbfc"
python_lib="./libs/python"
java_lib="./libs/java"

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


function python_lib() {
    while IFS= read -r pkg; do
        if [[ -n "$pkg" ]]; then
            echo "Installing: $pkg"
            sudo apt install -y "$pkg"
        fi
    done < "$python_lib"
}

function java_lib() {
    while IFS= read -r pkg; do
        if [[ -n "$pkg" ]]; then
            echo "Installing: $pkg"
            sudo apt install -y "$pkg"
        fi
    done < "$java_lib"
}

# Create Icon



echo "Running NBFC setup . . ."

# update_package
# install_libs
# install_nbfc
echo "Which programming language do you want to run with?"
select chose in "Java" "Python"; do
    case $chose in
        Java)
            echo "You chose Java"
            java_lib
            break
            ;;
        Python)
            echo "You chose Python"
            echo "Sorry, I haven't released it for Python yet, please choose Java."
            ;;
        *)
            echo "Invalid option. Please choose 1 or 2."
            ;;
    esac
done

