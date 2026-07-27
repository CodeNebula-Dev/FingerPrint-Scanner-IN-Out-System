# Fingerprint project data
This is markdown file of the fingerprint project covering the management of the database of the entire project.

This part is divided into these followimg parts :-

* Data input / File management
* Per day Database
* Accessing the data when needed 

## Data input / File management

Steps :-

- First we should define the students as struct datatypes having the following things :-
  
  1. string roll_number
  2. string name
  3. string year
  
    ```
    struct student{
        string roll_number;
        string name;
        int year;
        int phone_number;
        uint8_t fingerprint_template;
    };
    /* uint8_t is a special type of datatype which will help us to save and manage the fingerprint_templates using the library <cstdint> */
    ``` 
    *uint8_t (from <cstdint>) is explicitly 8 bits, so it's the natural choice for storing templates and serializing them to a file or database.*


 * Then we create a folder and save the file with their name.csv/name.txt .
 * We will make this file inside the batch file as batch_year.txt(example :-  2025_batch.txt ),which will contain the names of the student files so we could search each for each student,this will also help to auto update the batch of the student of the whole batch with just one click.

## Per day Database / File

* At first we should include the ctime library to the code which will detect the time of the system as follows.
```
#include <iostream>
#include <chrono>
#include <ctime>

int main() {
auto now = std::chrono::system_clock::now();
std::time_t t = std::chrono::system_clock::to_time_t(now);
// local time as human-readable string
std::cout << std::ctime(&t); 
return 0;
}
```
* If the time if 00:00 or passed the it will create a new file of that date (eg:- **01_01_2026.txt / 01_01_2026.csv**) where we shall save the records of that date.
```
#include <iostream>
#include <filesystem>
namespace fs = std::filesystem;

int main() {
    fs::path source = "old_folder/file.txt";
    fs::path destination = "new_folder/file.txt";

    try {
        fs::rename(source, destination);
        std::cout << "File moved successfully!";
    } catch (fs::filesystem_error& e) {
        std::cerr << "Error: " << e.what();
    }
}
```
* This date_file will be saved inside a month_file(eg:- **January.txt / January.csv**) , which will be saved inside a year file (eg:- **2026.txt / 2026.csv**) using the same logic as above maybe .
```
#include <filesystem>
namespace fs = std::filesystem;

// At program start, ensure the folder exists
fs::create_directories("students");

// Then save files inside it
string file_name = "students.txt";
```
and we can manipulate the location by using the same library as follows
```
fs::create_directory("Students");          // same folder as .exe
fs::create_directory("../Students");       // one level up
fs::create_directory("../../Students");    // two levels up
fs::create_directory("data/Students");     // inside a subfolder
```
* The code will ask be started when the admin/sub-admin starts it. When started the program will take the date from the system then check if there exixts any file of that date in the **everyday_data** folder. If there exists then the program will ask the admin/sub-admin to continue on the same date or to stop the program.
  * If the admin/sub-admin wants to continue on the same date the program will open the date file and continue writing on it .
  * else the program will simply return to the admin panel.
  * However if there dosen't exists any file of the same date. The program will create a new file in which it will make the records of the current date. 
* After this the program will end on 18:30 or when the admin/sub-admin clicks on the end option. (We cannot edit or do any thing to the records if the program is in its record maintaining phase.)
  * When the program is started, it will automatically ask for the fingerprint through the connected fingerprint scanner.
  * When it an input is given the program will search for the name of the student which will match the fingerprint. From all the prerecorded student record in the student_data file.
  * It will display the name of the student in one column using "bits/stdc++.h" header file like this.
```
#include <bits/stdc++.h>
using namespace std;

struct Student
{
    string name;
    string roll;
    string batch;
};

int main()
{
    ifstream fin("students.txt");
    if (!fin)
    {
        cerr << "Cannot open students.txt\n";
        return 1;
    }

    vector<Student> students;
    string line;
    while (getline(fin, line))
    {
        if (line.empty())
            continue;
        // parse CSV line: name,roll,batch (trim spaces)
        stringstream ss(line);
        string name, roll, batch;
        if (!getline(ss, name, ','))
            continue;
        if (!getline(ss, roll, ','))
            continue;
        if (!getline(ss, batch))
            batch = "";
        auto trim = [](string &s)
        {
            size_t a = s.find_first_not_of(" \t\r\n");
            size_t b = s.find_last_not_of(" \t\r\n");
            s = (a == string::npos) ? string() : s.substr(a, b - a + 1);
        };
        trim(name);
        trim(roll);
        trim(batch);
        students.push_back({name, roll, batch});
    }

    // determine column widths
    size_t w_name = 20, w_roll = 8, w_batch = 5; // minimum widths (header lengths)
    for (auto &s : students)
    {
        w_name = max(w_name, s.name.size());
        w_roll = max(w_roll, s.roll.size());
        w_batch = max(w_batch, s.batch.size());
    }

    // print header
    cout << left << setw(w_name + 2) << "Name"
         << left << setw(w_roll + 2) << "Roll"
         << left << setw(w_batch + 2) << "Batch" << '\n';
    // Instead of the cout it should be some txt filel which will be filled in the background.

    cout << string(w_name + w_roll + w_batch + 6, '=') << '\n';

    // print rows
    for (auto &s : students)
    {
        cout << left << setw(w_name + 2) << s.name
             << left << setw(w_roll + 2) << s.roll
             << left << setw(w_batch + 2) << s.batch << '\n';
    }
    // Instead of the cout it should be some txt filel which will be filled in the background.

    return 0;
}
```
  * It will check the **student_pass_through_gate_count**. 
    * If the **student_pass_through_gate_count** is odd (he/ she is going out) , then the program will collect the time from the system and display it in the out-time_column.Then add 1 to **student_pass_through_gate_count**.
    * If the **student_pass_through_gate_count** is even (he/ she is going in) , then the program will collect the time from the system and display it in the in_time_column.Then add 1 to **student_pass_through_gate_count**.
  * It will then display the year they are currently in , in a separate column.
  * It will ask for the purpose of leaving, the student has to choose amoung the four options 

    1. Market
    2. Medical
    3. Exam
    4. Home
      * If the student presses for home ,program will ask for the admin for the admin/subadmin to confirm in the admin pannel (when the leave form is submitted).If the admin/subadmin don't confirm then a messsage will be dislayed in the list that "The leave form is not submitted". And 1 will be substracted from **student_pass_through_gate_count** and that student record will be remooved from the everyday_list.
        ``` 
         If (purpose == "Home"){
            // 1. Allow
            // 2. Don't allow
            // code for that 
         }
        ```  
    5. Others
      * If the student presses for others, then they have to write the purpose through the keyboard.

* When the time hits 18:30 or when the admin/sub-admin will check for the out students, program will check the **student_pass_through_gate_count** of all the student who have gone out.
* If the **student_pass_through_gate_count** is still odd the will make a list of those student, and show them in the admin panel with an additional column of there phone numbers which will be collected from thr original database. Except those students who went to home.
* Then if the student is late then after they enter their fingerprint they will be automatically removed the list of out students. But there time will not change(Until the admin wants to).
* Then the admin/subadmin can turn off the program . 
* When the code is not running the the admin/sub-admin can edit/ update / do whatever they want to with the records via admin panel. 

## Accessing the data when needed

* There will be mainly two files 
  1. Student_data (containing the details of all the students)
  2. Everyday_data (containing the record of everyday)
* When the program if offline the admin can access to all the datas which have been recorded. The admin can later edit all the datas if they want.
    * In the admin they will have the edit option. From this they can edit the records by going to the files itself. 
* They have an option to add the student, delete the student, edit the student.
* The admin can promote the year of the students of one batch with one click. 

### Libraries included
---
1. cstdint :- Helps to store the finegerprint input from the device.
2. filesystem :- This library helps us to manipulate the location / rename the file.
3. chrono :- <chrono> provides types and utilities for durations, clocks, and time points (e.g., std::chrono::duration, std::chrono::time_point, std::chrono::system_clock) 
4. ctime :- <ctime> (the C-style time header) supplies functions to get and convert time_t and tm values (e.g., time, clock, difftime, mktime, localtime/gmtime, ctime, asctime, strftime).
5. bits/stdc++.h :- <bits/stdc++.h> is a non-standard GCC implementation header that includes most standard C and C++ library headers in one file; it provides no new functions itself — it simply pulls in the standard library headers so you can use their functions and types without writing many individual #include lines.
6. fstream :- helps in the creation/deletion of the txt/csv file.  

Thats for now.
