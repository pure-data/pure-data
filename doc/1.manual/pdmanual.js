/* (c) Lucas Cordiviola - lucarda27@hotmail.com
* this file works with "pdmanual.css" to allow the user
* to switch to "dark-mode" and also to switch to 
* "sans-serif" font.
* 
* Licensed under the MIT license - https://opensource.org/licenses/MIT
*/

function doDark () {
  document.body.classList.add("dark-theme");
  var i;
  var x;
  
  x = document.getElementsByTagName("P");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.add("dark-theme");
	}
	
  x = document.getElementsByTagName("h1");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.add("dark-theme");
	}
	
  x = document.getElementsByTagName("h2");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.add("dark-theme");
	}
	
  x = document.getElementsByTagName("h3");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.add("dark-theme");
	}
	
  x = document.getElementsByTagName("h4");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.add("dark-theme");
	}
	
  x = document.getElementsByTagName("h5");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.add("dark-theme");
	}

  x = document.getElementsByTagName("h6");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.add("dark-theme");
	}
	
  x = document.getElementsByTagName("ol");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.add("dark-theme");
	}
	
  x = document.getElementsByTagName("ul");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.add("dark-theme");
	}
	
  var x = document.getElementsByTagName("mark");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.add("dark-theme");
	}
	
  x = document.getElementsByTagName("PRE");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.add("dark-theme");
	}
	
  x = document.getElementsByTagName("IMG");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.add("dark-theme");
	}
	
  x = document.getElementsByTagName("a");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.add("dark-theme");
	}
	
  x = document.getElementsByTagName("CODE");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.add("dark-theme");
	}
	
  x = document.getElementsByTagName("tr");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.add("dark-theme");
	}
	
  x = document.getElementsByTagName("th");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.add("dark-theme");
	}
	
  x = document.getElementsByTagName("td");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.add("dark-theme");
	}
	
}

function doLight () {
  document.body.classList.remove("dark-theme");

  var i;
  var x;
  
  x = document.getElementsByTagName("P");
  var i;
  for (i = 0; i < x.length; i++) {
    x[i].classList.remove("dark-theme");
	}
	
  x = document.getElementsByTagName("h1");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.remove("dark-theme");
	}
	
  x = document.getElementsByTagName("h2");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.remove("dark-theme");
	}
	
  x = document.getElementsByTagName("h3");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.remove("dark-theme");
	}
	
  x = document.getElementsByTagName("h4");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.remove("dark-theme");
	}
	
  x = document.getElementsByTagName("h5");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.remove("dark-theme");
	}

  x = document.getElementsByTagName("h6");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.remove("dark-theme");
	}
	
  x = document.getElementsByTagName("ol");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.remove("dark-theme");
	}
	
  x = document.getElementsByTagName("ul");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.remove("dark-theme");
	}
	
  x = document.getElementsByTagName("mark");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.remove("dark-theme");
	}
	
  x = document.getElementsByTagName("PRE");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.remove("dark-theme");
	}
	
  x = document.getElementsByTagName("IMG");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.remove("dark-theme");
	}
	
  x = document.getElementsByTagName("a");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.remove("dark-theme");
	}
	
  x = document.getElementsByTagName("CODE");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.remove("dark-theme");
	}
	
  x = document.getElementsByTagName("tr");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.remove("dark-theme");
	}
	
  x = document.getElementsByTagName("th");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.remove("dark-theme");
	}
	
  x = document.getElementsByTagName("td");  
  for (i = 0; i < x.length; i++) {
    x[i].classList.remove("dark-theme");
	}
}

function doSerif () {	
  document.body.classList.remove("sans-serif");
  document.body.classList.add("serif");
}

function doSansserif () {	
  document.body.classList.remove("sans");
  document.body.classList.add("sans-serif");
  
  
  
}

const btntheme = document.querySelector(".btn-toggle-theme");
const currentTheme = localStorage.getItem("theme");



const btnfont = document.querySelector(".btn-toggle-font");
const currentBodyfont = localStorage.getItem("bodyFont");




console.log(currentTheme);
console.log(currentBodyfont);

if (currentTheme == "dark") {
  doDark();	
  } else {
	  doLight();
  }


if (currentBodyfont == "sans-serif") {
  doSansserif();	
  } else {
	  doSerif();
  }  


btntheme.addEventListener("click", function () {
  document.body.classList.toggle("dark-theme"); 

  let theme = "light";

  if (document.body.classList.contains("dark-theme")) {
    theme = "dark";
	doDark();
	} else {
		doLight(); 
  }
  localStorage.setItem("theme", theme);
});




btnfont.addEventListener("click", function () {
  document.body.classList.toggle("sans-serif"); 

  let bodyFont = "serif";

  if (document.body.classList.contains("sans-serif")) {
    bodyFont = "sans-serif";
	doSansserif();
	} else {
		doSerif(); 
  }
  localStorage.setItem("bodyFont", bodyFont);
});
