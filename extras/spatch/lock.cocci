@ alreadylocked exists @
expression lock;
identifier fn;
position lockedfn;
@@
 fn(...) {
 ...when != vlc_mutex_lock(&lock)
 vlc_mutex_unlock(&lock);
 ...
 vlc_mutex_lock@lockedfn(&lock);
 ...
 }
@ nobrace exists @
expression lock;
expression condition;
position p;
position unlockedfn != alreadylocked.lockedfn;
@@
 vlc_mutex_lock@unlockedfn(&lock);
 ...when != vlc_mutex_unlock(&lock);
 if@p (condition)
+{
+    vlc_mutex_unlock(&lock);
     return ...;
+}

@ braced exists @
expression lock;
expression condition;
position p != nobrace.p;
position unlockedfn != alreadylocked.lockedfn;
@@
 vlc_mutex_lock@unlockedfn(&lock);
 ...when != vlc_mutex_unlock(&lock);
 if@p (condition) {
     ...when != vlc_mutex_unlock(&lock);
+    vlc_mutex_unlock(&lock);
     return ...;
 }
