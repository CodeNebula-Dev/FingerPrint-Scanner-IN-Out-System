# Fingerprint Scanner

This project aim's to upgrade the manual entry system in collegs.
manual entry system are full of hustel ; for example you are going out and there is long qeue 
you have to wait until you turn comes and it will unnecessary take your and others time, 
thus to  overcome such pproblems we came up  with a idea to digitilised this system

## How do we want to proceed?

1. We have a main database which has all the details of the students saved, like: Name, fingerprint, or any other detail we want to associat with the student.  
2. Every day a new database would be created in a seperate database, saving each day's entery log.  
3. To save the space, the daily logs would be exported into excel.

## Step-1

This is the main base of our system. Saving all the details associated with the students allows the admin to access the details accordingly when needed.  
Details like: 'name', 'fingerprint', 'contact information', 'roll no' etc.  
This main database can be updated once a year when a new batch of students join the institute.  

## Step-2

We'll have a seperate database dedicated for these daily logs.  

           |-------------------------|
           |  first entry of the day |
           |-------------------------|
                        |
                        v
           |-------------------------|
           | table with the date as  |
           |    title is created     |
           |-------------------------|
                        |
                        v
           |-------------------------|
           |  is the fingerprint in  |
           |   the main database?    |
           |-------------------------|
                        |
              NO        |    YES
        ________________|________________
       |                                 |
       v                                 v
    |-----------|          |-------------------------|
    | Fail safe |          |  Name associated to it  |
    |-----------|          |  is fetched from the    |
                           |        database         |
                           |-------------------------|
                                         |
                                         v
                           |-------------------------|
                           | a new coloumn is created|
                           |        with this name   |
                           |-------------------------|
                                         |
                                         v
                           |-------------------------|
                           | Purpose: will be given  |
                           | to choose from: Market, |
                           | Exam, class, Home etc.  |
                           |-------------------------|
                                         |
                                         v
                                |-----------------|
                                |  chose HOME?    |
                                |-----------------|
                                         |
                              YES        |     NO
                        _________________|_________________
                       |                                   |
                       v                                   v
                |----------------|                |----------------|
                | The Admin will |                | the time of the|
                | approve        |                | entery is saved|
                |----------------|                | in the coloumn |
                                                  |----------------| 
                                                           |
                                                  |---------------|
                                                  | is a hosteler |
                                                  |---------------|
                                                           |
                                             YES           |    NO
                                       ____________________|_________________
                                      |                                      |
                                      v                                      v
                       |----------------------|                   |-----------------------|
                       | odd inputs = outside |                   |  odd inputs = inside  |
                       | even inputs = inside |                   | even inputs = outside |
                       |----------------------|                   |-----------------------|
                                      |                                      |
                                      |____________________|_________________|
                                                           |
                                                           v
                                           |------------------------------|
                                           | flags the status accordingly |
                                           |------------------------------|
                                                           |
                                                           |
                                                           v
                                          |-------------------------------|
                                          | when the curfew begins, list  |
                                          | of hostelers still outside and|
                                          | day-scholars still inside will|
                                          | be displayed                  |
                                          |-------------------------------|
                                                           
## Step-3

- when the day ends, the day's records are exported to an excel sheet and this table is reused for the next day.
OR
- Every day a new table is created, storing the day's logs. And at the end of the year, the whole year's data would be exported into an excel sheet and these tables will be reused for the next year.

This would both save space in our database and the records can be accessed whenevr needed.

## Examples

1. A student scans their fingerprint, their name is fetched and a row with their name is created.  They are then asked for the purpose and the student chooses, 'Market'. So the purpose is filled. The time stamp is recorded, say 11:30 hrs, and stored in the respective coloumn.Now, they are flagged as 'OUTSIDE'. The student comes back and scans their fingerprint again, now they are flagged as 'INSIDE', with the time stamp recorded again, say 17:30 hrs. 

2. A student scans their fingerprint, their name is fethced and a row with their name is created. They are then asked for the purpose and they choose, 'Exam'. So the purpose is filled. The time stamp is recorded, say 9:30 hrs, and stored in the respective coloumn. Now, they are flagged as 'OUTSIDE". It is already the curfew time, and the student is not back yet. The admin can see the list of students still outsidet the campus and can see this particular student's name. When the student comes back, say 19:30 hrs, the student scans again and is now flagged as 'INSIDE'.

3. A student who is a day scholar, scans their fingerprint with their purpose as 'class'. A new row with their name is created and time stamp is recorded, say 11:00 hrs. They are flagged as 'INSIDE'. When they leave they scan again and the time stamp is recorded, say 16:40 hrs. Now, their status is flagged as 'OUTSIDE'.

4. A student scans their fingerprint , their name is fetched and a row with their name is created . they are then ask for the purpuse and they choose ,'HOME'. so for  this case student needs approval from the admin , a request will be sent to  admin , if admin will allow then only student can go home otherwise  student have to stay inside . home approved status will be displayed on the screen and thier status is flagged as 'outside'


# Modifications

## Students who choose HOME

The earlier logic in 'idea.md' had a few flaws in the aspect of when a student comes back from HOME. So the new change is as follows:
- a new database would be created to store the details of the students who choose HOME.
- when the student returns and scans, it would first verify if they are in the HOME database, if so, the 'entry' coloumn would be filled and their name would be removed form the HOME database.

## Status logic

The status logic for day-scholors and hostelers were different, so the earlier logic in 'idea.md' didn't asist this, so an addition to the existing logic is as follows:
- a new coloumn in the main databse called 'role' would be added, which would state wether the student is a 'hosteler' or a 'non-hosteler'.
- whenever a studetn scans their fingerprint, along with their name and other details, their role would be fetched from the main database as well.
- if the role is 'hosteler', it'll follow the status logic for the hosteler and if it is 'noan-hosteler', the logic for the same would be followed.