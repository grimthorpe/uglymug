#!/bin/bash

# make sure we haven't called with no arguments
if ( test -z $1 ); then
	echo "usage: $0 <version>"
	echo "  where version is of the form: 1_xyz"
	exit
fi

# variables
WD=/tmp/umpkg

# colours
YEL="\033[1;33m"
RED="\033[1;31m"
RES="\033[0m"

# intro text
echo -e ""
echo -e "${RED}--------------------------------------------------------------------------------${RES}"
echo -e "                            UglyCODE Package Script"
echo -e "${RED}--------------------------------------------------------------------------------${RES}"
echo -e ""

# retrieve the command live argument(s)
VERSION=$1
# create x.yz format
DOTVERSION=`echo $VERSION | sed 's,_,\.,'`

# create a working directory
rm -rf ${WD}/UglyCODE
mkdir -p ${WD}
cd ${WD}

# check out the tagged versioN
export CVS_RSH=ssh
echo -e "${YEL}Press <ENTER> at the password prompt${RES}"
cvs -d:pserver:anonymous@cvs.uglycode.sourceforge.net:/cvsroot/uglycode login
# export the files to package; pipe the output into a file
echo -en "Exporting um$VERSION. Output from this is in ${WD}/package.${VERSION} ... "
cvs -z3 -d:pserver:anonymous@cvs.uglycode.sourceforge.net:/cvsroot/uglycode export -r um$VERSION UglyCODE 1>${WD}/package.${VERSION} 2>${WD}/package.${VERSION}
echo "done"

# only continue if /tmp/umpkg/UglyCODE exists
if [ -d ${WD}/UglyCODE ]; then
	# we don't want to put the CVS dir into the package - export instead of checkout
	# prevents the need for this
	#rm -rf UglyCODE/CVS

	# tar up the UglyCODE directory
	tar czf uglycode-${VERSION}.tar.gz UglyCODE

	# remove the UglyCODE directory
	rm -rf ${WD}/UglyCODE/

	# transfer the file to sourceforge
	echo -e "${RED}--------------------------------------------------------------------------------${RES}"
	echo -e "${YEL}Login as ${RED}anonymous${YEL} at the login prompt${RES}"
	echo -e "${YEL}Sorry, until I can bash script the actual FTP process, you'll need to run the following commands manually:${RES}"
	echo -e "  bin"
	echo -e "  passive"
	echo -e "  cd /incoming"
	echo -e "  put uglycode-${VERSION}.tar.gz"
	echo -e "  quit"

	# open ftp connection
	ftp upload.sourceforge.net

	echo -e ""
	echo -e ""
	echo -e "${RED}--------------------------------------------------------------------------------${RES}"
	echo -e "Now go to ${YEL}https://sourceforge.net/project/admin/editpackages.php?group_id=33517${RES} and:"
	echo -e "  add a release for the ${YEL}uglycode${RES} package"
	echo -e "  ${YEL}https://sourceforge.net/project/admin/newrelease.php?package_id=38404&group_id=33517${RES}"
	echo -e ""
	echo -e "The release should be called: ${YEL}${DOTVERSION}${RES}"
	echo -e ""
	echo -e "Paste the information for $VERSION from tag_list into the ${YEL}ChangeLog${RES} box"
	echo -e "  [${YEL}http://cvs.sourceforge.net/cgi-bin/viewcvs.cgi/uglycode/UglyCODE/tag_list${RES}]"
	echo -e ""
	echo -e "  select the uploaded file (${YEL}uglycode-${VERSION}.tar.gz${RES}) from the list of files"
	echo -e "  choose 'Any' and 'source .gz'"
	echo -e ""
	echo -e "Go to: ${YEL}https://sourceforge.net/projects/uglycode/${RES}"
	echo -e "The release should be listed. The file can take up to 30 minutes to appear on the download page."
	echo -e ""
	echo -e "${RED}--------------------------------------------------------------------------------${RES}"
	echo -e ""
	echo -e "When the file is available, please make sure that you complete Step 4 on the release page:"
	echo -e ""
	echo -e "  select \"I'm Sure\""
	echo -e "  click on \"Send Notice\""
	echo -e ""
	echo -e "${RED}--------------------------------------------------------------------------------${RES}"
else
	echo -e "${RED}The source code was not successfully checked out. Do some detective work, and re-run this script${RES}"
fi
