#!/bin/bash
echo "=== DEBUGGING FSTAB ISSUE ==="
echo "1. Checking if /mnt exists:"
ls -la /mnt/

echo "2. Checking if /mnt/etc exists:"
ls -la /mnt/etc/

echo "3. Looking for any fstab files:"
find /mnt -name "*fstab*" 2>/dev/null

echo "4. Checking what's mounted:"
findmnt | grep /mnt

echo "5. Trying to generate fstab manually:"
genfstab -U /mnt

echo "6. Checking if genfstab command exists:"
which genfstab

echo "=== END DEBUG ==="