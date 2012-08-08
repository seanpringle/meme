// Meme SCRIPTFILE example (Ctl+j)
// username/password autofill
// set this file's permissions to 600!

function id(s) { return document.getElementById(s); }
function name(s) { return document.getElementsByName(s)[0]; }
function auth(ut, uh, u, pt, ph, p)
{
	if (ut == 'id') id(uh).value = u; else name(uh).value = u;
	if (pt == 'id') id(ph).value = p; else name(ph).value = p;
}

// enter login credentials below
// view the target site page source to find the relevant username/password form inputs
var credentials = [
	// url regex      id/name  element  value                     id/name  element    value
	['google.com',    'id',    'Email', 'john.citizen',           'id',    'Passwd',  'qwerty' ], // look for <INPUT id="Email" ...> and <INPUT id="Passwd" ...>
	['facebook.com',  'id',    'email', 'john.citizen@gmail.com', 'id',    'pass',    'asdfgh' ],
]

var found = false;
for (var i in credentials)
{
	var row = credentials[i];
	var re  = new RegExp(row[0], 'i');
	if (document.location.href.match(re))
	{
		auth(row[1], row[2], row[3], row[4], row[5], row[6]);
		found = true;
		break;
	}
}
if (!found) alert('no match');
