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

   **students living in hostel**
   it will chcek how many  times the student living in hostel has crossed the gate. 

   - if the count is even ; then the student status will be IN (inside the institute)
   - if the count is odd ; then the  student status will be out (outside the instiute )

   **students who are daysholars**
   -  if the count is even; then the student status will be out .
   -  if the the count is odd; then  the student status will be in.

## step 3

At the end of the day  admin will end the session  (time set  by admin 18:30)
- programme will  check today's file and those  students whose count is still odd 
 and those  students will  appear in still outside list. 

- it will remove those students who got approval from admin for going home , their status will  be  out 

- students  who  return late can scann their fingerprint and they will be removed from the still outside list.

## step 4
after all these  cross checking  admin shuts the programme when it is done
the flies are saved  and  unlocked
