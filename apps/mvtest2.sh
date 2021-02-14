#!/bin/bash

print_status() {
    ret=$?

    if [[ $ret -eq 0 ]]; then
        echo "........PASSED........."
    else
        echo "........FAILED........."
    fi
}

echo -e "\nTESTING"

##### MAKE SURE INFO & LS WORK ON SINGLE BLK FILE ######
##### INFO SMALL FILE ######
dd if=/dev/urandom of=small.txt bs=100 count=1 status=none
echo -e "\ntest info small file..."

./fs_make.x test1.fs 100 > /dev/null
./fs_make.x test2.fs 100 > /dev/null

./test_fs.x add test1.fs small.txt > /dev/null
./test_fs.x add test2.fs small.txt > /dev/null

./test_fs.x info test1.fs> my_sm_info.txt
./test_fs.x info test2.fs> ref_sm_info.txt

diff my_sm_info.txt ref_sm_info.txt
print_status

##### LS SMALL FILE
echo -e "\ntesting ls small file..."

./test_fs.x ls test1.fs> my_sm_info.txt
./test_fs.x ls test2.fs> ref_sm_info.txt

diff my_sm_info.txt ref_sm_info.txt
print_status


# remove ls and sm files
rm -f my_sm_info.txt ref_sm_info.txt small.txt
#######################################################



# file just large enough to use 2 blocks
dd if=/dev/urandom of=random.txt bs=4097 count=1 status=none


###### BIGGER FILES ####################################
###### TEST LS ##########
echo -e "\ntesting ls with big file..."

# our file
./fs_make.x test1.fs 100 > /dev/null
./test_fs.x add test1.fs random.txt > /dev/null
./test_fs.x ls test1.fs> my_ls.txt

# tester file
./fs_make.x test2.fs 100 > /dev/null
./fs_ref.x add test2.fs random.txt > /dev/null
./fs_ref.x ls test2.fs > ref_ls.txt

diff my_ls.txt ref_ls.txt
print_status

# remove ls files
rm -f my_ls.txt ref_ls.txt



##### TEST INFO ##########
echo -e "\ntesting info with big file......."

# our file
./test_fs.x info test1.fs> my_info.txt

# tester file
./fs_ref.x info test2.fs > ref_info.txt

diff my_info.txt ref_info.txt
print_status

# remove info files
rm -f my_info.txt ref_info.txt



#### TEST READ ###########
echo -e "\ntesting read with one big file......"

# our file
./test_fs.x cat test1.fs random.txt > my_ls.txt

# tester file
./fs_ref.x cat test2.fs random.txt > ref_ls.txt

diff my_ls.txt ref_ls.txt
print_status



#### ADD SECOND BIG FILE ##########
echo -e "\nadding a second big file, comparing ls....."

dd if=/dev/urandom of=random2.txt bs=4097 count=1 status=none

# our file
./test_fs.x add test1.fs random2.txt > /dev/null
./test_fs.x ls test1.fs > my_ls.txt

# tester file
./fs_ref.x add test2.fs random2.txt > /dev/null
./fs_ref.x ls test2.fs > my_ls.txt

diff my_ls.txt ref_ls.txt
print_status

#### COMPARE INFO 2 BIG FILE ########
echo -e "\ncomparing info for 2 big files..."
./test_fs.x info test1.fs > my_ls.txt
./fs_ref.x info test2.fs > my_ls.txt

diff my_ls.txt ref_ls.txt
print_status


# TODO
# Filesystem full
# Read file (more than 1 block)
# Delete file (more than 1 block)
# 


#########################################################

# REMOVE ANY CREATED FILEs
rm -f ref_ls.txt my_ls.txt random*.txt test1.fs test2.fs > /dev/null
