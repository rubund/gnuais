#
# Regular cron jobs for the gnuais package
#
0 4	* * *	root	[ -x /usr/bin/gnuais_maintenance ] && /usr/bin/gnuais_maintenance
