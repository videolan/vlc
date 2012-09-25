#!/bin/bash

# A script checking the git logs for commits. Final goal is updating AUTHORS.
# Run it in source root

# To be copied and run in the git directory for having "git shortlog -sn po/" find the logs.
# It will generate a subdirectory temp_update_AUTHORS

OLD_AUTHORS="AUTHORS_unmodified_by_No_se_script.txt"
# The last version before modifying AUTHORS with this kind of script. Important, since the script shall not remove anyone.
# Not even the manual editings delete anyone.
# 4f696a88ec9544b98e22ee45e010869717608bd2  here did j-b start using the script

if [ -f $OLD_AUTHORS ]; then
 echo "Starting with credits from $OLD_AUTHORS ..."
else
 echo "You need an old AUTHORS file. Only if you know what you are doing, you can use the current AUTHORS."
 echo "Aborting..."
 exit
fi


mkdir -p temp_update_AUTHORS

echo "Checking all git logs"
git shortlog -sn -- > temp_update_AUTHORS/all_git.txt

echo "Checking "po only" git logs"
#git shortlog -sn po/ > temp_update_AUTHORS/po_git.txt
# This modified command identified more translators, leading to the removal of existing entries in AUTHORS, impossible by design.
# So, start with a AUTHORS version with validated entries.
# Only if the script is not modified (wrt finding translators) you can use the current AUTHORS
git shortlog -sn -- po extras/package/win32/languages/ share/vlc.desktop.in  share/vlc.desktop share/applications/vlc.desktop extras/package/win32/vlc.win32.nsi.in > temp_update_AUTHORS/po_git_previous.txt

# Some typical (ancient) l10n files:
#share/applications/vlc.desktop
#extras/package/win32/languages/declaration.nsh
#extras/package/win32/languages/english.nsh
#extras/package/win32/languages/french.nsh
#extras/package/win32/vlc.win32.nsi.in

sed -n '{
s/.*Song Ye Wen.*//
s/.*Florian Hubold.*//
s/.*Sveinung Kvilhaugsvik.*//
s/.*Julien Humbert.*//
/^$/ !p
}
' <temp_update_AUTHORS/po_git_previous.txt >temp_update_AUTHORS/po_git.txt

# Checking the logs, this ^^ persons did not do l10n
# commited to share/vlc.desktop, but this seems to be media types, not l10n things.
# Florian Hubold
# share/applications/vlc.desktop
# Sveinung Kvilhaugsvik
# commited to extras/package/win32/vlc.win32.nsi.in   and   include/vlc_interface.h
# Julien Humbert
# comitted to po/POTFILES.in
# Song Ye Wen

# TODO: add this newly discovered translators
# czech translator
# Radek Vybiral <radek@ns.snake.cz>
# zh_TW
# Thanks to Hsi-Ching Chao
# Thanks to Ruei-Yuan Lu <RueiYuan.Lu@gmail.com>
#extras/package/win32/languages/schinese.nsh
#share/vlc.desktop


# there are some artwork designers in git log, too. If one of them wants to be mentioned in "Programmers" also, remove here:
echo "Damien Erambert"  >  temp_update_AUTHORS/artwork_git.txt
echo "Daniel Dreibrodt" >> temp_update_AUTHORS/artwork_git.txt
echo "Dominic Spitaler" >> temp_update_AUTHORS/artwork_git.txt




echo "reading AUTHORS"
sed -n '/Programming/,/^$/  s/[^-].*/&/p' < $OLD_AUTHORS | sed '1 d'  > temp_update_AUTHORS/programmers_part.txt
# The part of AUTHORS between Programming and the first empty line, without the ---- line


echo "Removing commit counts from git log"
sed 's/[0-9 \t]*\(.*\)/\1/g' < temp_update_AUTHORS/all_git.txt |sort|uniq > temp_update_AUTHORS/all_git_namesonly.txt
# I think "uniq" is not needed here.


echo "Removing translators from the git log"
# Remove translators. (Commiters with the same count in /po and total and hence are listed twice). Then the commit counter is removed
cat temp_update_AUTHORS/all_git.txt temp_update_AUTHORS/po_git.txt|sort|uniq -u |sed 's/[0-9 \t]*\(.*\)/\1/g' | sort|uniq> temp_update_AUTHORS/coders_only.txt


####
#For script tuning: Are there other files the translators modified? =>probably l10n files
cat temp_update_AUTHORS/all_git.txt temp_update_AUTHORS/po_git.txt|sort|uniq -u| sed 's/[0-9 \t]*\(.*\)/\1/g' | sort|uniq -d> temp_update_AUTHORS/both_sides.txt
####

# Similar effect with second sed run:
# Remove translators. I remove every line containing the name. Maybe the .* before and after the last \1 should be removed (i.e. for contributors "Firstname Secondname aka something_you_want_to_keep"
#cat temp_update_AUTHORS/all_git.txt temp_update_AUTHORS/po_git.txt|sort|uniq -D|uniq|sed 's/[0-9 \t]*\(.*\)/\1/g' |sed 's:[0-9 \t]*\(.*\):s^.*\1.*^^g:' > temp_update_AUTHORS/remove_translators_gen
#
#sed -f temp_update_AUTHORS/remove_translators_gen < temp_update_AUTHORS/all_git_namesonly.txt > temp_update_AUTHORS/coders_only.txt
# This is everyone who did code commits with git. The blank lines are the removed translators.


# Now, I want to reduce the number of lines the human reader has to check, so we are going to kill the already listed contributors.

echo "Finding pre-git contributors in AUTHORS"
sed 's:\(.*\):s^.*\1.*^^g:' < temp_update_AUTHORS/coders_only.txt > temp_update_AUTHORS/remove_git_commiters_gen
sed -f temp_update_AUTHORS/remove_git_commiters_gen < temp_update_AUTHORS/programmers_part.txt |sort| uniq -u > temp_update_AUTHORS/pre-git.txt

sed 's:\(.*\):s^.*\1.*^^g:' < temp_update_AUTHORS/programmers_part.txt > temp_update_AUTHORS/remove_programmers_part_gen
sed -f temp_update_AUTHORS/remove_programmers_part_gen < temp_update_AUTHORS/coders_only.txt |sort| uniq -u > temp_update_AUTHORS/new_coders_only.txt

# VideoLAN as a contributor can be removed, I think



sed 's/[0-9 \t]*\(.*\)/\1/g' < temp_update_AUTHORS/all_git.txt  > temp_update_AUTHORS/all_git_namesonly_ordered.txt
# Just remove the tab an the # commits, keep the order. This file is going to be the sort order.
# You can re-order the complete programmers part like that and simply append the pre-git commiters.
# Ordering the contributors that way is not a bad idea.
# The question: Is it easier/better to check the new commiters in this order?
# One can find (UPPERCASE issues, middle names,..) better when listing them alphabetically.

# I suggest checking manually a file build like that:
# alphabetically ordered, complete list of contributors/git (code) commiters, with an extra marking for new ones. Example
# Old Commiter
# New Commiter           ---XXX---NEW
# New COMMITER           ---XXX---NEW   |same name with UPPERCASE part
# Very Commiter
# Very New Commiter      ---XXX---NEW   |Same person with a middle name

# The uppercase case can be done by script I guess, I did not look up how to make sure the intended version will be picked.
# How to proceed with manually found problems? Solve them for the future (.mailmap/own script/...)

rm -f temp_update_AUTHORS/ordering_log.txt
rm -f temp_update_AUTHORS/ordered_by_commits.txt 
FileName='temp_update_AUTHORS/all_git_namesonly_ordered.txt' 
while read LINE
do
 if [ "$LINE" = "VideoLAN" ]; then
  echo "VideoLAN is not a person"
 else
#  grep "$LINE" temp_update_AUTHORS/new_coders_only.txt >> temp_update_AUTHORS/ordering_log.txt
  grep "$LINE" temp_update_AUTHORS/coders_only.txt >> temp_update_AUTHORS/ordering_log.txt
#  grep "$LINE" temp_update_AUTHORS/coders_only.txt >> temp_update_AUTHORS/ordering_log.txt
# I want to keep the $? (it removes some broken names) but I could send the output to /dev/null
# If someone's name is a prefix to some other's name, this diff will show it:
# diff temp_update_AUTHORS/ordering_log.txt temp_update_AUTHORS/ordered_by_commits.txt
# AFAIK this will not effect the output, since we don't use the grep output but only the git output
  if [ $? = "0" ]; then
    grep "$LINE" temp_update_AUTHORS/artwork_git.txt ||  echo "$LINE" >> temp_update_AUTHORS/ordered_by_commits.txt
  fi
 fi
done < $FileName



cat temp_update_AUTHORS/all_git.txt temp_update_AUTHORS/po_git.txt|sort|uniq -D|uniq|sed 's/[0-9 \t]*\(.*\)/\1/g' > temp_update_AUTHORS/translators.txt
wc -l temp_update_AUTHORS/*
echo "Some contributors only commited into po. Please cross-check that with the localization part. See: temp_update_AUTHORS/translators.txt"
echo "But first, please check if temp_update_AUTHORS/review.txt contains complete names and other constraints for publishing (i.e. UPPERCASE name parts, broken text, a name and it's abbreviation both present...)"


sed 's/\(.*\)/\1               ---XXX---NEW/g' < temp_update_AUTHORS/new_coders_only.txt | cat - temp_update_AUTHORS/programmers_part.txt |sort > temp_update_AUTHORS/review.txt
# This file contains VideoLAN as a contributor.

echo
echo "For the lazy ones: Have a look at temp_update_AUTHORS/final.txt"
echo "Contains all git code commiters (the translators are stored somewhere else) sorted by commits, and the pre-git commiters"
echo
echo 'For the lazy and brave (stupid?) ones: "cp new_AUTHORS AUTHORS" and check "git diff AUTHORS"'

echo "Programming" >  temp_update_AUTHORS/final.txt
echo "-----------" >> temp_update_AUTHORS/final.txt
cat temp_update_AUTHORS/ordered_by_commits.txt temp_update_AUTHORS/pre-git.txt >> temp_update_AUTHORS/final.txt
echo

echo "Listing email adresses used with different names..."
git shortlog -sne |sed 's/[^<]*\(.*\)/\1/g' |sort|uniq -d
echo "If something was listed here you should probably modify .mailmap"

# This last part puts the relevant addresses into temp_twice_used_adress/check_for_this.txt
# Currently, this is not needed (.mailmap is up to date)

#mkdir -p temp_twice_used_adress
#echo "Checking all git logs"
#git shortlog -sne > temp_twice_used_adress/all_shortlog_sne.txt
#
#echo "Removing everything but email addresses"
#sed 's/[^<]*\(.*\)/\1/g' < temp_twice_used_adress/all_shortlog_sne.txt |sort|uniq -d > temp_twice_used_adress/all_git_addresses_only.txt
#
#FileName='temp_twice_used_adress/all_git_addresses_only.txt'
#while read LINE
#do
#  grep "$LINE" temp_twice_used_adress/all_shortlog_sne.txt >> temp_twice_used_adress/check_for_this.txt
## I want to keep the $? (it removes some broken names) but I could send the output to /dev/null
#  if [ $? = "0" ]; then
#    echo "$LINE"
#  fi
#done < $FileName



sed -n '1,2  p' < AUTHORS   > temp_update_AUTHORS/new_AUTHORS
cat temp_update_AUTHORS/final.txt >> temp_update_AUTHORS/new_AUTHORS
echo >>temp_update_AUTHORS/new_AUTHORS
sed -n '/Artwork/,$ p' < AUTHORS >> temp_update_AUTHORS/new_AUTHORS

cp temp_update_AUTHORS/new_AUTHORS .
#rm -rf temp_update_AUTHORS/
