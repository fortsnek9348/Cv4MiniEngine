New-Item -ErrorAction Ignore log.txt
Get-Content log.txt -Wait -Tail 30