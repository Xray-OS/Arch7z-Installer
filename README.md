# Arch7z Installer (Xray_OS)

## 🚧 Note

Arch7z Installer is in **alpha**. Expect bugs, missing features, and rough edges. Proceed with caution.

---

## 🚀 Overview

Arch7z Installer replaces Calamares for Xray_OS with a lightweight, Qt-based installer written in C++. It focuses on speed, simplicity, and a clean user experience for both real hardware and virtual machines.

---

## Key Features

- Fast C++/Qt implementation  
- Simple, step-by-step UI  
- Full locale configuration:
  - Language  
  - Region  
  - Timezone  
  - Keyboard layout and variant with live testing  
- Two installation modes:
  - Clean Install (wipes target disk)  
  - Custom Install (manual partitioning via built-in options and GParted)  
- Filesystem support: EXT4 or BTRFS  
- VM-friendly single-partition layout  

---

## Installation Modes

### Clean Install

1. Select the target disk.  
2. Installer automatically partitions EFI/boot and root.  
3. Choose filesystem (EXT4 or BTRFS).  
4. Proceed with installation.  

### Custom Install

1. Launch GParted from within the installer to modify partitions.  
2. Click **Refresh** to reload your changes.  
3. Assign partitions for:
   - EFI/boot (required)  
   - root `/` (required)  
   - swap (optional)  
4. Select filesystem types.  
5. Continue with installation.  

---

## User Configuration

On the final screen, set up your system user and defaults:

- Hostname (default: `Xray_OS`)  
- Username and password  
- Optionally use the same password for root or set a separate one  
- Shell choice: Fish (default), Bash, or Zsh  

---

## Build & Run

1. Install dependencies: Qt 5/6, CMake, GCC or Clang.  
2. Clone the repository:
   ```bash
   git clone https://github.com/Xray-OS/arch7z-installer.git
   cd arch7z-installer

   - mkdir build && cd build

   - Run CMake to configure the project Assuming your Qt installation is properly set up and your environment knows where to find it:
   - CMAKE:
    	"cmake .. -DCMAKE_PREFIX_PATH=$(qmake -query QT_INSTALL_PREFIX)/lib/cmake"


   - If you’re using Qt6, you might need: 
    	"cmake .. -DCMAKE_PREFIX_PATH=$(qtpaths --qt-version 6 --install-prefix)/lib/cmake"
   
    - Build the project
	    "cmake --build"
   		"make"

     - Run the project
       "sudo ./arch7z-installer"
   

**Arch7Z installer is still in alpha** there is a lot of stuff to fix, so is functional you can also install Xray_OS through Arch7z Installer, but be aware that the software is still in alpha and there is still a lot of stuff to fix and cover.

 
