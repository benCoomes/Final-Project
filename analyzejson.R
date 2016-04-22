library(jsonlite)
library(magrittr)

# for every file in the test_data directory
path <- "/home/ben/cpsc3600/Final-Project/bmgeorg/test_data/"
filelist <- list.files(path)

i <- 1
timedata <- {}
for (file in filelist){
  # access json file and convert to list of vectors
  fullpath <- paste(path, file, sep = "")
  print(fullpath)
  timedata[[i]] <- fromJSON(fullpath)
  for (name in names(timedata[[i]])){
    timedata[[i]][[name]] <- timedata[[i]][[name]] %>% unlist()
  }
  i <- i + 1
}

combined_data <- {}
#for every list of lists 
for(datalist in timedata){
  # for every sublist
  for(name in names(datalist)){
    combined_data[[name]] <- c(combined_data[[name]], datalist[[name]])
  }
}

means <- {}
totals <- {}
for(name in names(combined_data)){
  means[name] <- mean(combined_data[[name]])
  totals[name] <- sum(combined_data[[name]])
}
