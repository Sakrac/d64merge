# d64merge
Merge filenames from a 1541 data disk with a corresponding art disk. For use with lovingly crafted DirArt d64s to easily apply to d64 disks with boringly named files.

Usage:
d64merge.exe \<optional switches\> \<d64 data\> \<d64 dir\> \<d64 out\>

Optional Switches:
* -skip=n Don't assign data files to directory n files after the first entry
* -first=n Set first file at index n
* -list Show directory of data, dir and result
* -append Append the dir name after the data name instead of overwriting initial part of dir name with data name
* -chain Print the track/sector sequence for the directory and each file.

Notes:
* data / dir / out need to be different files
* dir d64 should have equal or more files than data d64.

## Background

I receieved a wonderfully crafted d64 directory layout for Space Moguls and asked how I'd go about applying it to the real disk. I didn't think the method suggested sounded very simple so I made a basic tool to achieve what I needed.
Basically the loading is done with a two character filename and a wildcard ('\*') so extending the filenames from the original disk is ok. The tool will perform this for each file from the original disk and appending the remaining characters from the directory disk.
