#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/sched/signal.h>
#include <linux/timer.h>

#define PROC_FILENAME "app_running_time"
#define MAX_ENTRIES 200 // maximum number of applications
#define BUFFERSIZE 1000

#define INTERVAL 5000

static struct timer_list process_timer;
static struct proc_dir_entry *proc_entry;

// application information
static char *commands[MAX_ENTRIES];
static char *name[MAX_ENTRIES];
static int update[MAX_ENTRIES];
static int is_running[MAX_ENTRIES];

static int arr[MAX_ENTRIES]; // for sorting applications by time
static unsigned long running_time_ms[MAX_ENTRIES];

// module load time
static unsigned long load_time;

// number of applications
static int commands_size = 0;

// Read a line from a file given the offset
ssize_t read_line_from_file(struct file *file, loff_t *f_pos, char *line, int len){
    char buffer[1];
    ssize_t index = 0;
    while(kernel_read(file, buffer, 1, f_pos) == 1){
        if(buffer[0] != '\n' && index < len){
            line[index] = buffer[0];
            index++;
        }
        else{
            return index;
        }
    }
    return 0;
}   

// Read .desktop files, find command from the "Exec=" line and name from "Name=" line
static void parse_desktop_files(void) {
    struct file *app_dir, *file;
    struct dentry *dentry, *curdentry;

    int SIZE = 1000, SKIP = 5; // maximum line size
    int last_slash, first_space;
    char line[1000];
    char *DIR = "/usr/share/applications";
    ssize_t read;
    loff_t f_pos;  
    int len, app_name_len, app_cmd_len;

    char app_name[BUFFERSIZE];
    char app_cmd[BUFFERSIZE];    

    bool found_app_name, found_app_cmd, repeated;     

    // Open the directory containing .desktop files
    app_dir = filp_open(DIR, O_RDONLY, 0);
    if (IS_ERR(app_dir)) {
        printk(KERN_ERR "Failed to open directory\n");
        return;
    }

    dentry = app_dir->f_path.dentry;
    list_for_each_entry(curdentry, &dentry->d_subdirs, d_child) {
        char file_path[100] = "/usr/share/applications/";
        strcat(file_path, curdentry->d_name.name);

        file = filp_open(file_path, O_RDONLY, 0);
        if (IS_ERR(file)) {
            printk(KERN_ERR "Failed to open file%s\n", file_path);
            continue;
        }
        f_pos = 0;
        
        
        found_app_name=false;
        found_app_cmd=false;
        
        app_name_len=0;
        app_cmd_len=0;
        
        while ((!found_app_name || !found_app_cmd) && (read = read_line_from_file(file, &f_pos, line, SIZE)) > 0) {
            /*
                Suppose the line is
                Exec=/usr/bin/firefox %u .....
                We want "firefox", so find first space after '=' and last slash before the first space
            */
            if (!found_app_cmd && read >= 5 && line[0] == 'E' && line[1] == 'x' && line[2] == 'e' && line[3] == 'c' && line[4] == '=') {
                first_space = SKIP;
                while(first_space < read && line[first_space] != ' ')
                    first_space++;
                last_slash = first_space - 1;
                while(last_slash >= SKIP && line[last_slash] != '/')
                    last_slash--;
                len = first_space - last_slash - 1;
                if(len <= 0)
                    break;
                
                // store the command
                strncpy(app_cmd, line + last_slash + 1, len);
                app_cmd[len] = '\0';
                app_cmd_len=len;
                found_app_cmd = true;
            }
            
            
            if (!found_app_name && read >= 5 && line[0] == 'N' && line[1] == 'a' && line[2] == 'm' && line[3] == 'e' && line[4] == '=') {
                len=read-5;
                app_name_len=len;
                found_app_name=true;

                // store the application name
                strncpy(app_name, line + 5, len);   
                app_name[len] = '\0';             
            }
        }
        
        repeated = false;
        for(int i = 0; i < commands_size; i++){
            if(strcmp(commands[i], app_cmd) ==0){
                repeated = true;
                break;
            }
        }
        
        if(repeated)
            continue;
        
        commands[commands_size] = kmalloc(app_cmd_len + 1, GFP_KERNEL);
        name[commands_size] = kmalloc(app_name_len + 1, GFP_KERNEL);
        strncpy(name[commands_size], app_name, app_name_len);
        strncpy(commands[commands_size], app_cmd, app_cmd_len);
        printk("%s %s %s %d %d",file_path,name[commands_size],commands[commands_size],app_cmd_len,app_name_len);
        commands_size++;
       
        
        if (commands_size >= MAX_ENTRIES) {
            printk(KERN_WARNING "Reached maximum number of application entries\n");
            return;
        }
        
        // filp_close(file, NULL);
    }
    // for(int i = 0; i < commands_size; i++)
    //     printk("%s", commands[i]);

    filp_close(app_dir, NULL);
}

void update_time(struct timer_list *timer) {
    // struct task_struct *task;

    struct task_struct *task;

    printk("----------------------Updating times of applications---------------------------------");
    for_each_process(task){
    
        for(int i = 0; i < commands_size; i++){
            if(strcmp(task->comm, commands[i]) == 0 && update[i]==0){
                running_time_ms[i] += INTERVAL;
                update[i]=1;
                printk("%s\n", task->comm);
            }
        }
    }
    
    for(int i = 0; i < commands_size; i++){
        is_running[i] = update[i];
        update[i]=0;
    }
    mod_timer(&process_timer, jiffies + msecs_to_jiffies(INTERVAL));
}

static ssize_t read_proc(struct file *filp, char __user *buffer, size_t length, loff_t *offset) {
    unsigned long seconds, minutes, hours;
    /*
    printk(KERN_INFO "-------------------------------___APP INFO -----------------------------------------");
    printk(KERN_INFO "Process Name\tRunning Time (hh:mm:ss)");

    for (int i = 0; i < commands_size; ++i) {
        if(running_time_ms[i] == 0)
                continue;
        // Convert milliseconds to hours, minutes, and seconds
        seconds = running_time_ms[i] / 1000;
        
        minutes = seconds / 60;
        hours = minutes / 60;
        seconds %= 60;
        minutes %= 60;

        printk(KERN_INFO "%-30s\t%02lu:%02lu:%02lu\t\n", commands[i], hours, minutes, seconds);
    }
    */
    
    int len=0;
    char buf[BUFFERSIZE];
    char *running;
    int tmp;
    
    if(*offset>0)
        return 0;


    for(int i = 0; i < commands_size; i++){
        arr[i] = i;
    }

    // sort based on running time
    for(int i = 0; i < commands_size; i++){
        for(int j = commands_size - 1; j >= 1; j--){
            if(running_time_ms[arr[j]] > running_time_ms[arr[j - 1]]){
                tmp = arr[j];
                arr[j] = arr[j - 1];
                arr[j - 1] = tmp;
            }
        }
    }
    
    len+=sprintf(buf+len, "------------------------------- APPLICATION USAGE -----------------------------------------\n\n");
    len+=sprintf(buf+len, "%-40s\tTIME (hh:mm:ss)\t\tIS_RUNNING\n\n", "APPLICATION");

    for (int i = 0; i < commands_size; ++i) {
        if(running_time_ms[arr[i]] == 0)
                continue;
        // Convert milliseconds to hours, minutes, and seconds
        seconds = running_time_ms[arr[i]] / 1000;
        
        minutes = seconds / 60;
        hours = minutes / 60;
        seconds %= 60;
        minutes %= 60;
        
        running = (is_running[arr[i]] ? "Yes" : "No");
        len+=sprintf(buf+len, "%-40s\t%02lu:%02lu:%02lu\t\t%s\n", name[arr[i]], hours, minutes, seconds, running);
    }
    
    if(copy_to_user(buffer,buf,len))
        return -EFAULT;
    
    *offset=len;
    
    return len;
}

static ssize_t write_proc(struct file *filp, const char  *buf, size_t len, loff_t *off) 
{
    printk(KERN_INFO "Writing...");
    return 0;
}

static const struct proc_ops proc_fops = {
    //.owner = THIS_MODULE,
    //.proc_open = open_proc,
    .proc_read = read_proc,
    .proc_write = write_proc, 
    //.proc_release = release_proc
};

static int __init init_app_running_time(void) {

    proc_entry = proc_create(PROC_FILENAME, 0, NULL, &proc_fops);
    if (!proc_entry)
        return -ENOMEM;

    // Load commands from .desktop files
    parse_desktop_files();
    
    load_time=jiffies_to_msecs(jiffies);

    // Initialize the timer
    timer_setup(&process_timer, update_time, 0);
    mod_timer(&process_timer, jiffies + msecs_to_jiffies(INTERVAL));
    printk(KERN_INFO "Application running time module loaded...\n");

    return 0;
}

static void __exit exit_app_running_time(void) {
    proc_remove(proc_entry);

    del_timer(&process_timer);
    printk(KERN_INFO "Application running time module unloaded. Bye...\n");
}

module_init(init_app_running_time);
module_exit(exit_app_running_time);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rakshith, Abhishek and Akash");
