# Builds all the projects in the solution...
.PHONY: all_projects
all_projects: CAR_TRACKING 

# Builds project 'CAR_TRACKING'...
.PHONY: CAR_TRACKING
CAR_TRACKING: 
	make --directory="CAR_TRACKING/" --file=CAR_TRACKING.makefile

# Cleans all projects...
.PHONY: clean
clean:
	make --directory="CAR_TRACKING/" --file=CAR_TRACKING.makefile clean

